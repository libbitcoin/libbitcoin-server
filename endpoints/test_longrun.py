"""
Long-running requests: concurrency, cancellation and shutdown.

Heavy requests run on the parallel pool and abort through the protocol's
stopping flag once the channel stops. A channel stops when the peer is seen
to close, which for a request in flight relies on the peer-close monitor
(network channel::monitor, polled every 10 ms). These tests measure:

- cancellation: after the client closes (fin) or aborts (rst), the server's
  cpu must return to idle well before the request would have completed;
- isolation: while heavy requests run, a light request on any service is
  answered within LIGHT_LATENCY;
- partial cancellation: closing some of several concurrent heavy requests
  leaves the others to complete correctly;
- shutdown: stopping the node with heavy requests in flight completes in
  bounded time (opt-in, --run-shutdown, restart the node afterwards).

Requires a synced mainnet store (reference: the genesis-block address with
its many dust outputs) and --bs-pid (cpu is read from /proc, same host).

Run with:
    pytest test_longrun.py -s --bs-pid=$(pidof bs) \\
      --robust-services=native,electrum,sparrow,bitcoind --sparrow-port=50003
    pytest test_longrun.py -s --run-slow ...      # includes multi-minute runs
"""

from __future__ import annotations

import json
import os
import socket
import struct
import threading
import time

import pytest

from robustness import (REACT, close, connect, http_done, http_request,
                        http_split, read, reset, send, survived)

# The genesis-block address (1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa) scripthash,
# in electrum (reversed) byte order, also used by the native address target.
GENESIS_SCRIPTHASH = \
    "8b01df4e368ea28f8dc0423bcf7a4923e3a12d307c875e47a0cfbf90b5c39161"

# A light request on another connection must be answered within this.
LIGHT_LATENCY = 2.0

# Cpu (cores) above the idle baseline that still counts as idle.
IDLE_MARGIN = 1.0

# Cpu (cores) below which the node counts as quiet before a measurement.
QUIET = 2.0


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


@pytest.fixture
def cpu(robust_env):
    if not robust_env.process:
        pytest.skip("cpu measurement requires --bs-pid (same host)")
    return Cpu(robust_env.process.pid)


class Cpu:
    """Process cpu in cores, from /proc (all threads)."""

    def __init__(self, pid: int):
        self.pid = pid
        self.hz = os.sysconf("SC_CLK_TCK")

    def ticks(self) -> int:
        total = 0
        for tid in os.listdir(f"/proc/{self.pid}/task"):
            try:
                with open(f"/proc/{self.pid}/task/{tid}/stat") as f:
                    fields = f.read().rsplit(")", 1)[1].split()
                total += int(fields[11]) + int(fields[12])
            except (OSError, IndexError, ValueError):
                pass
        return total

    def cores(self, window: float = 1.0) -> float:
        before = self.ticks()
        time.sleep(window)
        return (self.ticks() - before) / self.hz / window

    def idle(self, limit: float = 900) -> float:
        """Baseline after earlier work has drained (e.g. an uncancelled scan
        of a previous test): the lowest of three one-second samples, once
        three consecutive samples are below QUIET cores."""
        start, quiet = time.monotonic(), 0
        while quiet < 3 and time.monotonic() - start < limit:
            quiet = quiet + 1 if self.cores() < QUIET else 0
        return min(self.cores() for _ in range(3))

    def time_to_idle(self, baseline: float, limit: float) -> float:
        """Seconds until two consecutive samples are idle, or limit."""
        start, quiet = time.monotonic(), 0
        while time.monotonic() - start < limit:
            quiet = quiet + 1 if self.cores() <= baseline + IDLE_MARGIN else 0
            if quiet == 2:
                return time.monotonic() - start - 2.0
        return limit


# ─── Heavy and light requests per service ────────────────────────────────────

def bitcoind_body(method, params=None, ident=1) -> bytes:
    return json.dumps({"jsonrpc": "1.0", "id": ident, "method": method,
                       "params": [] if params is None else params}).encode()


def electrum_line(method, params=None, ident=1) -> bytes:
    return (json.dumps({"jsonrpc": "2.0", "id": ident, "method": method,
                        "params": [] if params is None else params}) +
            "\n").encode()


class Heavy:
    """A connection carrying long-running work. bitcoind runs one scan of
    minutes; electrum/sparrow/native pipeline many heavy requests, drained
    by a reader thread so the server keeps computing."""

    def __init__(self, svc, count: int = 200):
        self.svc = svc
        self.sock = connect(svc.host, svc.port)
        self.received = 0
        self.thread = None
        if svc.name == "bitcoind":
            body = bitcoind_body("gettxoutsetinfo", ["none"])
            send(self.sock, http_request(
                "POST", "/", svc.authority, body,
                [("Content-Type", "application/json")] + svc.auth_headers()))
            return
        if svc.electrum:
            send(self.sock, electrum_line("server.version",
                                          ["longrun", svc.protocol], 0))
            read(self.sock, REACT, lambda d: b"\n" in d)
            send(self.sock, b"".join(
                electrum_line("blockchain.scripthash.get_history",
                              [GENESIS_SCRIPTHASH], i + 1)
                for i in range(count)))
        elif svc.name == "native":
            send(self.sock, b"".join(
                http_request("GET", f"/v1/address/{GENESIS_SCRIPTHASH}"
                             "?format=json", svc.authority)
                for _ in range(count)))
        else:
            pytest.skip(f"no heavy request defined for {svc.name}")
        self.thread = threading.Thread(target=self._drain, daemon=True)
        self.thread.start()

    def _drain(self) -> None:
        self.sock.settimeout(None)
        try:
            while True:
                chunk = self.sock.recv(1 << 20)
                if not chunk:
                    return
                self.received += len(chunk)
        except OSError:
            return

    def abort(self, how: str) -> None:
        abort(self.sock, how)


def start_heavy(svc, count: int = 200) -> Heavy:
    return Heavy(svc, count)


def light_latency(svc) -> float:
    """Seconds for one light request on a fresh connection (inf on error)."""
    start = time.monotonic()
    sock = connect(svc.host, svc.port)
    try:
        if svc.electrum:
            send(sock, electrum_line("server.version",
                                     ["longrun", svc.protocol], 0))
            out = read(sock, LIGHT_LATENCY * 5, lambda d: b"\n" in d)
            ok = b'"result"' in out.data
        elif svc.name == "bitcoind":
            send(sock, http_request("POST", "/", svc.authority,
                                    bitcoind_body("getblockcount"),
                                    [("Content-Type", "application/json")] +
                                    svc.auth_headers()))
            out = read(sock, LIGHT_LATENCY * 5, http_done)
            ok = http_split(out.data) is not None
        else:
            send(sock, svc.http_probe())
            out = read(sock, LIGHT_LATENCY * 5, http_done)
            ok = http_split(out.data) is not None
    finally:
        close(sock)
    return time.monotonic() - start if ok else float("inf")


def abort(sock: socket.socket, how: str) -> None:
    """Close with a fin (shutdown, then close) or abort with a rst."""
    try:
        if how == "rst":
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                            struct.pack("ii", 1, 0))
        else:
            sock.shutdown(socket.SHUT_RDWR)
    except OSError:
        pass
    close(sock)


HEAVY = ["native", "electrum", "sparrow", "bitcoind"]

# Heavy work that a rst also cancels. A bitcoind scan aborted by rst runs to
# completion (the peer-close monitor detects only a fin), so storms leave it
# out rather than pile up scans that cannot be stopped.
CANCELLABLE = ["native", "electrum", "sparrow"]

# Why a rst does not cancel a bitcoind scan (until fixed in network).
RST_GAP = ("network asio::half_closed() detects a fin (TCP_CLOSE_WAIT) but "
           "not a rst (TCP_CLOSE), so the scan runs to completion")


def configured(robust_env, names) -> list:
    served = [service.name for service in robust_env.services]
    return [robust_env.service(name) for name in names if name in served]


@pytest.fixture(params=HEAVY)
def svc(request, robust_env):
    return robust_env.service(request.param)


# ─── Cancellation ────────────────────────────────────────────────────────────

@pytest.mark.parametrize("how", ["fin", "rst"])
def test_cancel_stops_work(svc, cpu, how):
    """The server stops working for a client that is gone."""
    baseline = cpu.idle()
    heavies = [start_heavy(svc) for _ in range(1 if svc.name == "bitcoind"
                                                else 4)]
    time.sleep(3)
    busy = cpu.cores(2)
    if busy <= baseline + IDLE_MARGIN:
        for heavy in heavies:
            heavy.abort(how)
        pytest.skip(f"{svc.name}: request not heavy enough to measure "
                    f"({busy:.1f} cores)")

    for heavy in heavies:
        heavy.abort(how)
    seconds = cpu.time_to_idle(baseline, 60)
    print(f"\n{svc.name} {how}: busy {busy:.1f} cores, idle after "
          f"{seconds:.1f}s")
    if svc.name == "bitcoind" and how == "rst" and seconds >= 5:
        # Let the uncancelled scan finish so later tests start quiet.
        cpu.idle(limit=1800)
        pytest.xfail(RST_GAP)

    assert seconds < 5, \
        f"{svc.name}: work continued {seconds:.0f}s after {how} " \
        f"({busy:.1f} cores before)"


# ─── Isolation ───────────────────────────────────────────────────────────────

def test_light_requests_served_during_heavy_load(robust_env, cpu):
    """Heavy work on every heavy service must not delay light requests."""
    socks = [start_heavy(svc)
             for svc in configured(robust_env, HEAVY)
             for _ in range(1 if svc.name == "bitcoind" else 4)]
    try:
        time.sleep(2)
        worst: dict = {}
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            for service in robust_env.services:
                latency = light_latency(service)
                worst[service.name] = max(worst.get(service.name, 0.0),
                                          latency)
            time.sleep(0.5)
        print(f"\nload {cpu.cores():.1f} cores, worst light latency: " +
              ", ".join(f"{k} {v:.2f}s" for k, v in worst.items()))
        slow = {k: v for k, v in worst.items() if v > LIGHT_LATENCY}
        assert not slow, f"light requests delayed by heavy load: {slow}"
    finally:
        for heavy in socks:
            heavy.abort("fin")


def test_cancel_storm(robust_env, cpu):
    """Many heavy requests started and abandoned at once (fin and rst)."""
    heavies = configured(robust_env, CANCELLABLE)
    baseline = cpu.idle()
    for round_ in range(3):
        socks = [start_heavy(svc) for svc in heavies for _ in range(8)]
        time.sleep(1)
        for index, heavy in enumerate(socks):
            heavy.abort("fin" if index % 2 else "rst")
    seconds = cpu.time_to_idle(baseline, 120)
    print(f"\ncancel storm: idle after {seconds:.1f}s")
    for service in robust_env.services:
        assert light_latency(service) <= LIGHT_LATENCY, service.name
    assert seconds < 10, f"work continued {seconds:.0f}s after the storm"


# ─── Partial cancellation (multi-minute) ─────────────────────────────────────

@pytest.mark.slow
def test_cancel_some_of_concurrent_scans(robust_env, cpu):
    """Three concurrent gettxoutsetinfo, two closed: the third completes."""
    svc = robust_env.service("bitcoind")
    socks = [start_heavy(svc) for _ in range(3)]
    time.sleep(5)
    for heavy in socks[:2]:
        heavy.abort("fin")

    survivor = socks[2].sock
    survivor.settimeout(None)
    start = time.monotonic()
    out = read(survivor, 1800, http_done)
    close(survivor)
    parsed = http_split(out.data)
    print(f"\nsurvivor answered after {time.monotonic() - start:.0f}s: "
          f"{out.data[:200]!r}")
    assert parsed and parsed[0] == 200, str(out)
    value = json.loads(parsed[2])
    assert value.get("error") is None, value
    assert value["result"]["txouts"] > 0


@pytest.mark.slow
def test_full_scan_result(robust_env):
    """A complete gettxoutsetinfo (hash_type none) returns a result."""
    svc = robust_env.service("bitcoind")
    sock = start_heavy(svc).sock
    sock.settimeout(None)
    start = time.monotonic()
    out = read(sock, 1800, http_done)
    close(sock)
    parsed = http_split(out.data)
    print(f"\nfull scan after {time.monotonic() - start:.0f}s: "
          f"{out.data[:300]!r}")
    assert parsed and parsed[0] == 200, str(out)
    value = json.loads(parsed[2])
    assert value.get("error") is None, value


# ─── Shutdown with work in flight (opt-in) ───────────────────────────────────

def test_shutdown_with_heavy_requests(robust_env, request):
    """SIGINT with heavy requests in flight: the node stops serving in bounded
    time. Not restarted; run last and restart the node by hand."""
    if not request.config.getoption("--run-shutdown"):
        pytest.skip("stops the node, use --run-shutdown")
    if not robust_env.process:
        pytest.skip("requires --bs-pid")
    socks = [start_heavy(svc)
             for svc in configured(robust_env, HEAVY) for _ in range(2)]
    time.sleep(3)
    ports = [(s.host, s.port) for s in robust_env.services]
    start = time.monotonic()
    os.kill(robust_env.process.pid, 2)

    def listening() -> bool:
        for host, port in ports:
            try:
                close(socket.create_connection((host, port), timeout=1))
                return True
            except OSError:
                pass
        return False

    while listening() and time.monotonic() - start < 300:
        time.sleep(0.5)
    seconds = time.monotonic() - start
    for heavy in socks:
        reset(heavy.sock)
    print(f"\nservices closed {seconds:.1f}s after SIGINT")
    robust_env.process = None       # the node is gone: skip survived()
    robust_env.services = []
    assert seconds < 60, f"shutdown with heavy requests took {seconds:.0f}s"
