"""
Robustness of the esplora service: REST targets over http GET/POST and the
same targets as websocket text messages.

Covers hostile targets (traversal, encoding, numeric edges, oversize),
broadcast bodies (POST /tx, /txs/package), large responses to clients that
stop reading or vanish, and websocket target abuse.

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_esplora.py --robust-services=esplora
"""

from __future__ import annotations

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
    return robust_env.service("esplora")


def get(svc, target, headers=()) -> tuple:
    out = exchange(svc.host, svc.port,
                   [http_request("GET", target, svc.authority,
                                 headers=headers)],
                   done=http_done, wait=3 * REACT)
    return http_split(out.data), out


def top_height(svc) -> int:
    parsed, out = get(svc, "/blocks/tip/height")
    assert parsed and parsed[0] == 200, str(out)
    return int(parsed[2].strip())


# ─── Hostile targets ─────────────────────────────────────────────────────────

GENESIS = ReferenceData.GENESIS_HASH
GENESIS_TX = ReferenceData.GENESIS_TX_HASH

TARGETS = [
    "/../../../../etc/passwd",
    "/%2e%2e/%2e%2e/%2e%2e/etc/passwd",
    "/block-height/-1",
    "/block-height/4294967296",
    "/block-height/18446744073709551616",
    "/block-height/" + "9" * 10000,
    "/block-height/%00",
    "/block-height/1e3",
    "/block/" + "f" * 63,
    "/block/" + "f" * 65,
    "/block/" + "z" * 64,
    "/block/" + GENESIS + "/txid/4294967296",
    "/block/" + GENESIS + "/txid/-1",
    "/block/" + GENESIS + "/txs/4294967295",
    "/block/" + GENESIS + "/txs/-25",
    "/block/" + GENESIS + "/txs/1",
    "/block/" + GENESIS + "/bogus",
    "/blocks/4294967296",
    "/blocks/-1",
    "/blocks/" + "1" * 1000,
    "/tx/" + "0" * 64 + "/outspend/4294967296",
    "/tx/" + GENESIS_TX + "/outspend/-1",
    "/tx/" + GENESIS_TX + "/merkle-proof/x",
    "/tx/" + "zz" * 32,
    "/address/" + "1" * 10000,
    "/address/bc1" + "q" * 5000,
    "/address/" + ReferenceData.EXAMPLE_ADDRESS + "/txs/chain/" + "f" * 63,
    "/address/" + ReferenceData.EXAMPLE_ADDRESS + "/txs/chain/" + "z" * 64,
    "/scripthash/" + "0" * 63,
    "/scripthash/" + "0" * 10000,
    "/mempool/txids/x",
    "/fee-estimates/x",
    "//blocks/tip/height",
    "/blocks/tip/height/",
    "/blocks/tip/height?x=" + "y" * 10000,
    "/blocks/tip/height?" + "&".join(f"k{i}=v" for i in range(10000)),
    "/blocks/tip/height#frag",
    "/" + "a/" * 5000,
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


@pytest.mark.parametrize("method", ["PUT", "DELETE", "PATCH", "OPTIONS",
                                    "HEAD"])
def test_other_methods(svc, method):
    request = http_request(method, "/blocks/tip/height", svc.authority,
                           b"{}" if method in ("PUT", "PATCH") else b"")
    out = exchange(svc.host, svc.port, [request],
                   done=http_done if method != "HEAD" else
                   (lambda d: b"\r\n\r\n" in d))
    assert_reacts(out, method)


# ─── Broadcast bodies ────────────────────────────────────────────────────────

BODIES = {
    "empty": b"",
    "not_hex": b"zz" * 100,
    "odd_hex": b"0" * 101,
    "binary": bytes(range(256)) * 16,
    "short_tx": b"00",
    "huge_hex": b"ff" * 2000000,
    "json": b'{"hex":"00"}',
    "nul_bytes": b"\x00" * 1000,
}


@pytest.mark.parametrize("path", ["/tx", "/txs/package"])
@pytest.mark.parametrize("case", sorted(BODIES))
def test_broadcast_body(svc, path, case):
    request = http_request("POST", path, svc.authority, BODIES[case],
                           [("Content-Type", "text/plain")])
    out = exchange(svc.host, svc.port, [request], done=http_done,
                   wait=3 * REACT)
    assert_reacts(out, f"POST {path} {case}")
    parsed = http_split(out.data)
    if parsed:
        assert parsed[0] < 500, f"{case}: server error {parsed[0]}"


# ─── Large responses ─────────────────────────────────────────────────────────

def large_targets(svc) -> list:
    height = min(top_height(svc), 700000)
    hashes = []
    for h in (height, max(0, height - 1)):
        parsed, _ = get(svc, f"/block-height/{h}")
        if parsed and parsed[0] == 200:
            hashes.append(parsed[2].decode().strip())
    targets = [f"/blocks/{height}"]
    for block in hashes:
        targets += [f"/block/{block}/raw", f"/block/{block}/txids",
                    f"/block/{block}/txs"]
    return targets


def test_large_response_never_read(svc):
    sock = connect(svc.host, svc.port, rcvbuf=4096)
    try:
        for target in large_targets(svc) * 5:
            send(sock, http_request("GET", target, svc.authority))
        time.sleep(WAIT)
        parsed, out = get(svc, "/blocks/tip/height")
        assert parsed and parsed[0] == 200, f"others not served: {out}"
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
        pytest.skip(f"esplora websocket unavailable ({exc})")


@pytest.mark.parametrize("target", [
    " ", "/", "/bogus", "blocks/tip/height", "/blocks/tip/height\n",
    "/tx", "/tx\n", "/tx\n" + "zz" * 1000, "/block-height/" + "9" * 10000,
    "x" * 1000000, "/blocks/tip/height\x00", "GET /blocks/tip/height HTTP/1.1",
    "/../../etc/passwd",
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
        ws.send(ws_text("/blocks/tip/height") * count)
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
        parsed, out = get(svc, "/blocks/tip/height")
        assert parsed and parsed[0] == 200, f"others not served: {out}"
    finally:
        ws.reset()
