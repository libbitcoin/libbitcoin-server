"""
Robustness of the native service: REST targets over http and the same
targets as websocket text messages (native.websocket = true), including the
subscription targets that push block, top and transaction events.

Covers hostile targets and queries (traversal, encoding, numeric edges,
oversize), media negotiation edges, large responses to clients that stop
reading or vanish, and subscribers that never read their pushes.

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_native.py
    pytest test_robustness_native.py --bs-pid=$(pgrep -f 'bs.*--config')
"""

from __future__ import annotations

import json
import time

import pytest

from robustness import (REACT, WAIT, CLOSED, OP_TEXT, RawWs, SILENT,
                        assert_reacts, connect, exchange, http_done,
                        http_request, http_split, read, reset, run_parallel,
                        send, stream, survived, ws_text)
from utils import ReferenceData


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


@pytest.fixture
def svc(robust_env):
    return robust_env.service("native")


def get(svc, target, headers=()) -> tuple:
    out = exchange(svc.host, svc.port,
                   [http_request("GET", target, svc.authority,
                                 headers=headers)],
                   done=http_done, wait=3 * REACT)
    return http_split(out.data), out


def top_height(svc) -> int:
    parsed, out = get(svc, "/v1/top?format=json")
    assert parsed and parsed[0] == 200, str(out)
    value = json.loads(parsed[2])
    return value if isinstance(value, int) else value[0]["height"]


# ─── Hostile targets ─────────────────────────────────────────────────────────

TARGETS = [
    "/v1/../../../../etc/passwd",
    "/v1/%2e%2e/%2e%2e/%2e%2e/etc/passwd",
    "/..%2f..%2f..%2fetc%2fpasswd",
    "/v1/block/height/%00",
    "/v1/block/height/%zz",
    "/v1/block/height/%",
    "/v1/block/height/-1",
    "/v1/block/height/4294967296",
    "/v1/block/height/18446744073709551616",
    "/v1/block/height/99999999999999999999999999999999999999",
    "/v1/block/height/1e3",
    "/v1/block/height/0x10",
    "/v1/block/height/ 1",
    "/v1/block/height/" + "1" * 10000,
    "/v1/block/hash/" + "f" * 63,
    "/v1/block/hash/" + "f" * 65,
    "/v1/block/hash/" + "z" * 64,
    "/v1/block/hash/" + "F" * 64,
    "/v1/block/height/0/tx/4294967296",
    "/v1/block/height/0/tx/-1",
    "/v1/block/height/0/filter/255",
    "/v1/block/height/0/filter/x/hash",
    "/v1/tx/" + "0" * 64 + "/output/4294967295",
    "/v1/tx/" + "0" * 64 + "/input/99999999999",
    "/v1/tx/" + ReferenceData.GENESIS_TX_HASH + "/output/0/spenders",
    "/v1/address/" + "0" * 64 + "/balance",
    "/v1/address/" + "0" * 63,
    "/v1/address/" + "0" * 64 + "/subscribe",
    "/v1/top/subscribe",
    "/v1/block/subscribe",
    "/v1",
    "/v1/",
    "/v1//top",
    "/v1/top/",
    "/v1/top//",
    "/v1/top/x/y/z/" + "a/" * 5000,
    "/v99/top",
    "/V1/TOP",
    "/v1/top#fragment",
    "/v1/top?",
    "/v1/top?format=",
    "/v1/top?format=xml",
    "/v1/top?format=json&format=text",
    "/v1/top?format=json&witness=maybe",
    "/v1/top?witness=" + "1" * 10000,
    "/v1/top?" + "&".join(f"k{i}=v" for i in range(10000)),
    "/v1/top?%zz=%zz",
    "/v1/top?format=%00",
    "//etc/passwd",
    "/./v1/./top",
    "/v1/top;param",
    "*",
]


@pytest.mark.parametrize("target", TARGETS,
                         ids=[f"t{i}" for i in range(len(TARGETS))])
def test_hostile_target(svc, target):
    parsed, out = get(svc, target)
    assert_reacts(out, target[:80])
    assert b"root:" not in out.data, "file content leaked"
    if parsed:
        assert parsed[0] < 500, f"{target[:80]}: server error {parsed[0]}"


@pytest.mark.parametrize("accept", [
    "", "*/*", "application/json;q=0", "text/html, application/json",
    "application/octet-stream", "text/plain", "image/png",
    "application/" + "x" * 10000, ";;;;", "*/*;q=abc",
])
def test_accept_header_edges(svc, accept):
    parsed, out = get(svc, "/v1/top", [("Accept", accept)])
    assert_reacts(out, f"accept {accept[:40]!r}")


def test_html_fallthrough_paths(svc):
    """Non-interface targets fall through to page dispatch."""
    for target in ("/", "/index.html", "/favicon.ico", "/../../etc/passwd",
                   "/%2e%2e/%2e%2e/etc/passwd", "/" + "a" * 10000):
        parsed, out = get(svc, target)
        assert_reacts(out, target[:40])
        assert b"root:" not in out.data, f"{target[:40]}: file leaked"


@pytest.mark.parametrize("method", ["POST", "PUT", "DELETE", "PATCH",
                                    "OPTIONS", "HEAD"])
def test_other_methods_on_interface(svc, method):
    request = http_request(method, "/v1/top?format=json", svc.authority,
                           b"{}" if method in ("POST", "PUT", "PATCH")
                           else b"")
    out = exchange(svc.host, svc.port, [request],
                   done=http_done if method != "HEAD" else
                   (lambda d: b"\r\n\r\n" in d))
    assert_reacts(out, method)


# ─── Large responses ─────────────────────────────────────────────────────────

def large_targets(svc) -> list:
    """Targets with the largest responses available at the current top."""
    height = min(top_height(svc), 700000)
    return [f"/v1/block/height/{h}?format=json"
            for h in (height, max(0, height - 1), max(0, height - 2))] + \
        [f"/v1/block/height/{height}/txs?format=json",
         f"/v1/block/height/{height}?format=data"]


def test_large_response_never_read(svc):
    sock = connect(svc.host, svc.port, rcvbuf=4096)
    try:
        for target in large_targets(svc):
            send(sock, http_request("GET", target, svc.authority))
        time.sleep(WAIT)
        parsed, out = get(svc, "/v1/top?format=json")
        assert parsed and parsed[0] == 200, f"others not served: {out}"
        time.sleep(WAIT)
    finally:
        reset(sock)


def test_large_response_read_slowly(svc):
    target = large_targets(svc)[0]
    sock = connect(svc.host, svc.port, rcvbuf=4096)
    try:
        send(sock, http_request("GET", target, svc.authority,
                                headers=[("Connection", "close")]))
        data, deadline = b"", time.monotonic() + 30
        while time.monotonic() < deadline:
            out = read(sock, 1.0, limit=8192)
            data += out.data
            if out.closed or http_done(data):
                break
            time.sleep(0.05)
        assert data, "nothing received"
    finally:
        reset(sock)


def test_disconnect_during_large_response(svc):
    targets = large_targets(svc)
    for i in range(30):
        sock = connect(svc.host, svc.port)
        send(sock, http_request("GET", targets[i % len(targets)],
                                svc.authority))
        if i % 2:
            read(sock, 0.05)
        reset(sock)


def test_parallel_large_responses(svc):
    targets = large_targets(svc)

    def work(index):
        for target in targets:
            parsed, out = get(svc, target)
            assert out.kind != SILENT, f"{target}: {out}"

    errors = run_parallel(16, work)
    assert not [e for e in errors if isinstance(e, AssertionError)], \
        errors[:3]


# ─── Websocket targets ───────────────────────────────────────────────────────

def open_ws(svc, **kwargs) -> RawWs:
    try:
        return RawWs(svc.host, svc.port, **kwargs)
    except ConnectionError as exc:
        pytest.skip(f"native websocket disabled? ({exc})")


@pytest.mark.parametrize("target", [
    "", " ", "\n", "/", "/v1", "/v1/bogus", "v1/top", "/v1/top\n/v1/top",
    "/v1/top?format=html", "/v1/top?format=text", "/v1/top?format=data",
    "/v1/../../etc/passwd", "/v1/block/height/" + "9" * 10000,
    "x" * 1000000, "/v1/top\x00", "GET /v1/top HTTP/1.1",
])
def test_bad_ws_target(svc, target):
    ws = open_ws(svc)
    try:
        ws.text(target)
        outcome = ws.outcome()
        if outcome == SILENT and not target.strip():
            pytest.skip("empty message ignored")
        assert outcome != SILENT, f"{target[:40]!r}: no reaction"
    finally:
        ws.close()


def test_ws_pipelined_targets(svc):
    ws = open_ws(svc)
    try:
        count = 200
        ws.send(ws_text("/v1/top?format=json") * count)
        for index in range(count):
            got = ws.message()
            assert got not in (None, CLOSED) and got[0] == OP_TEXT, \
                f"message {index}: {got!r}"
    finally:
        ws.close()


def test_ws_large_responses_never_read(svc):
    ws = open_ws(svc, rcvbuf=4096)
    try:
        burst = b"".join(ws_text(t) for t in large_targets(svc)) * 20
        stream(ws.sock, b"", burst, 16 * 1024 * 1024, stall=1.0)
        time.sleep(WAIT)
        parsed, out = get(svc, "/v1/top?format=json")
        assert parsed and parsed[0] == 200, f"others not served: {out}"
    finally:
        ws.reset()


@pytest.mark.parametrize("targets", [
    ["/v1/top/subscribe"],
    ["/v1/block/subscribe"],
    ["/v1/top/subscribe", "/v1/block/subscribe"],
    ["/v1/block/subscribe?format=data"],
])
def test_subscription_never_read(svc, targets):
    """Subscribe on a syncing node and stop reading: pushes must be bounded
    by the channel backlog while other clients are served normally."""
    ws = open_ws(svc, rcvbuf=4096)
    try:
        for target in targets:
            ws.text(target)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            time.sleep(2)
            start = time.monotonic()
            parsed, out = get(svc, "/v1/top?format=json")
            assert parsed and parsed[0] == 200, f"others not served: {out}"
            assert time.monotonic() - start < REACT
    finally:
        ws.reset()


def test_subscribe_repeatedly(svc):
    ws = open_ws(svc)
    try:
        ws.send(b"".join(ws_text(t) for t in
                         ["/v1/top/subscribe", "/v1/block/subscribe",
                          "/v1/tx/subscribe"] * 300))
        assert ws.outcome() != SILENT
    finally:
        ws.close()


def test_subscription_churn(svc):
    def work(index):
        for round_ in range(10):
            try:
                ws = RawWs(svc.host, svc.port)
            except (OSError, ConnectionError):
                continue
            ws.text("/v1/block/subscribe" if round_ % 2 else
                    "/v1/top/subscribe")
            (ws.reset if index % 2 else ws.close)()

    run_parallel(12, work)
