"""
Robustness of the websocket layer (libbitcoin-network websocket) on every
service that upgrades: native (with native.websocket = true), electrum and
btcd.

Covers rfc6455 violations a server must reject (unmasked or reserved frames,
bad control frames, invalid utf-8, absurd lengths), legal but unusual framing
a server must accept (fragmentation, control frames between fragments, empty
fragments), and abusive volume (ping floods, oversized messages, readers that
stop reading).

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_ws.py
    pytest test_robustness_ws.py --bs-pid=$(pgrep -x bs) -k btcd
"""

from __future__ import annotations

import os
import struct
import time

import pytest

from robustness import (REACT, WAIT, CLOSED, REPLY, SILENT, STALLED,
                        OP_BINARY, OP_CLOSE, OP_CONT, OP_PING, OP_PONG,
                        OP_TEXT, RawWs, assert_reacts, connect, exchange,
                        http_done, http_status, read, reset, run_parallel,
                        send, stream, survived, ws_frame, ws_key, ws_text,
                        ws_upgrade)

SERVICES = ["native", "esplora", "admin", "electrum", "sparrow", "bitcoind",
            "btcd"]


@pytest.fixture(params=SERVICES)
def svc(request, robust_env):
    return robust_env.service(request.param)


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


def open_ws(svc, **kwargs) -> RawWs:
    try:
        return RawWs(svc.host, svc.port, **kwargs)
    except ConnectionError as exc:
        pytest.skip(f"{svc.name}: no websocket ({exc})")


def answered(svc, ws: RawWs, wait: float = REACT) -> bool:
    got = ws.message(wait)
    return got not in (None, CLOSED) and got[0] == OP_TEXT and \
        svc.ws_ok(got[1])


def ends(ws: RawWs, wait: float = REACT) -> bool:
    """The server ends the connection (close frame or tcp end)."""
    return ws.drained_closed(wait)


# ─── Handshake ───────────────────────────────────────────────────────────────

@pytest.mark.parametrize("case", [
    "no_key", "short_key", "bad_key", "version_8", "version_99",
    "no_version", "post_method", "http10", "connection_without_upgrade",
    "upgrade_other_protocol", "key_with_body",
    "huge_key",
])
def test_bad_upgrade_gets_reaction(svc, case):
    key = ws_key()
    if case == "no_key":
        request = ws_upgrade("/", svc.authority, key=False)
    elif case == "short_key":
        request = ws_upgrade("/", svc.authority, key="AAAA")
    elif case == "bad_key":
        request = ws_upgrade("/", svc.authority, key="!!!!not base64!!!!==")
    elif case == "huge_key":
        request = ws_upgrade("/", svc.authority, key="A" * 100000)
    elif case == "version_8":
        request = ws_upgrade("/", svc.authority, key, version="8")
    elif case == "version_99":
        request = ws_upgrade("/", svc.authority, key, version="99")
    elif case == "no_version":
        request = ws_upgrade("/", svc.authority, key, version=None)
    elif case == "post_method":
        request = ws_upgrade("/", svc.authority, key, method="POST")
    elif case == "http10":
        request = ws_upgrade("/", svc.authority, key).replace(
            b"HTTP/1.1", b"HTTP/1.0", 1)
    elif case == "connection_without_upgrade":
        request = ws_upgrade("/", svc.authority, key).replace(
            b"Upgrade: websocket\r\n", b"", 1)
    elif case == "upgrade_other_protocol":
        request = ws_upgrade("/", svc.authority, key).replace(
            b"Upgrade: websocket", b"Upgrade: h2c", 1)
    else:
        request = ws_upgrade("/", svc.authority, key).replace(
            b"\r\n\r\n", b"\r\nContent-Length: 5\r\n\r\nhello", 1)
    out = exchange(svc.host, svc.port, [request], done=http_done)
    assert_reacts(out, case)


def test_upgrade_with_compression_offer(svc):
    """permessage-deflate offered: accepted or ignored, and the server then
    frames consistently with what it accepted."""
    ws = open_ws(svc, headers=[("Sec-WebSocket-Extensions",
                                "permessage-deflate; client_max_window_bits")])
    try:
        ws.text(svc.ws_probe())
        assert ws.outcome() != SILENT
    finally:
        ws.close()


def test_frame_bytes_inside_upgrade_segment(svc):
    """The first frame arrives in the same segment as the upgrade request."""
    sock = connect(svc.host, svc.port)
    try:
        send(sock, ws_upgrade("/", svc.authority) + ws_text(svc.ws_probe()))
        out = read(sock, REACT, lambda d: d.count(b"\r\n\r\n") >= 1 and
                   len(d.split(b"\r\n\r\n", 1)[1]) > 2)
        assert http_status(out) == 101, str(out)
    finally:
        reset(sock)


def test_raw_http_after_upgrade(svc):
    ws = open_ws(svc)
    try:
        ws.send(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
        assert ends(ws), "unframed http after upgrade was not rejected"
    finally:
        ws.close()


# ─── Protocol violations (must close) ────────────────────────────────────────

VIOLATIONS = {
    "unmasked_text": lambda p: ws_text(p, mask=False),
    "rsv1_without_extension": lambda p: ws_text(p, rsv=4),
    "rsv3": lambda p: ws_text(p, rsv=1),
    "reserved_opcode_3": lambda p: ws_frame(3, p.encode()),
    "reserved_opcode_b": lambda p: ws_frame(0x0b, b""),
    "continuation_first": lambda p: ws_frame(OP_CONT, p.encode()),
    "ping_over_125": lambda p: ws_frame(OP_PING, b"x" * 126),
    "fragmented_ping": lambda p: ws_frame(OP_PING, b"x", fin=False),
    "close_one_byte": lambda p: ws_frame(OP_CLOSE, b"\x03"),
    "close_bad_code_999": lambda p: ws_frame(OP_CLOSE, struct.pack("!H", 999)),
    "close_reserved_1005": lambda p: ws_frame(OP_CLOSE,
                                              struct.pack("!H", 1005)),
    "close_bad_utf8_reason": lambda p: ws_frame(
        OP_CLOSE, struct.pack("!H", 1000) + b"\xff\xfe"),
    "invalid_utf8_text": lambda p: ws_text(b"\xff\xfe\xfd"),
    "overlong_utf8_text": lambda p: ws_text(b"\xc0\xaf"),
    "surrogate_utf8_text": lambda p: ws_text(b"\xed\xa0\x80"),
    "text_inside_fragmented_text": lambda p: (
        ws_text(p[:2], fin=False) + ws_text(p[2:])),
    "non_minimal_length_16": lambda p: (
        bytes([0x81, 0x80 | 126]) + struct.pack("!H", 5) + b"\0\0\0\0" +
        b"hello"),
    "length_with_msb_set": lambda p: (
        bytes([0x81, 0x80 | 127]) + struct.pack("!Q", 1 << 63) + b"\0\0\0\0"),
}


@pytest.mark.parametrize("case", sorted(VIOLATIONS))
def test_protocol_violation_closes(svc, case):
    ws = open_ws(svc)
    try:
        ws.send(VIOLATIONS[case](svc.ws_probe()))
        assert ends(ws), f"{case}: connection still open"
    finally:
        ws.close()


def test_huge_declared_length_is_not_awaited(svc):
    """A frame declaring 2^62 bytes must be refused (read_message_max), not
    waited for or allocated."""
    ws = open_ws(svc)
    try:
        ws.send(bytes([0x81, 0x80 | 127]) + struct.pack("!Q", 1 << 62) +
                b"\0\0\0\0" + b"x" * 1024)
        assert ends(ws), "server waits for a 4 EiB frame"
    finally:
        ws.close()


def test_message_above_maximum_request(svc):
    """One message larger than the request limit, streamed in fragments."""
    ws = open_ws(svc)
    try:
        chunk = ws_text(b" " * 65536, fin=False)
        more = ws_frame(OP_CONT, b" " * 65536, fin=False)
        state, sent = stream(ws.sock, chunk, more, 64 * 1024 * 1024)
        if state == STALLED:
            state = CLOSED if read(ws.sock, REACT).kind != SILENT else STALLED
        assert state in (REPLY, CLOSED), \
            f"no reaction after a {sent} byte fragmented message ({state})"
    finally:
        ws.reset()


def test_single_frame_above_maximum_request(svc):
    ws = open_ws(svc)
    try:
        header = bytes([0x81, 0x80 | 127]) + struct.pack("!Q", 32 << 20) + \
            b"\0\0\0\0"
        state, sent = stream(ws.sock, header, b" " * 65536, 32 << 20)
        if state == STALLED:
            state = CLOSED if read(ws.sock, REACT).kind != SILENT else STALLED
        assert state in (REPLY, CLOSED), \
            f"no reaction after {sent} bytes of a 32 MiB frame ({state})"
    finally:
        ws.reset()


# ─── Legal but unusual framing (must work) ───────────────────────────────────

def test_fragmented_request(svc):
    probe = svc.ws_probe().encode()
    parts = [probe[i:i + 3] for i in range(0, len(probe), 3)]
    ws = open_ws(svc)
    try:
        frames = ws_frame(OP_TEXT, parts[0], fin=False)
        for part in parts[1:-1]:
            frames += ws_frame(OP_CONT, part, fin=False)
        frames += ws_frame(OP_CONT, parts[-1])
        ws.send(frames)
        assert answered(svc, ws)
    finally:
        ws.close()


def test_fragments_with_empty_and_control_frames_between(svc):
    probe = svc.ws_probe().encode()
    half = len(probe) // 2
    ws = open_ws(svc)
    try:
        ws.send(ws_frame(OP_TEXT, b"", fin=False))
        ws.send(ws_frame(OP_PING, b"between"))
        ws.send(ws_frame(OP_CONT, probe[:half], fin=False))
        ws.send(ws_frame(OP_PONG, b"unsolicited"))
        ws.send(ws_frame(OP_CONT, b"", fin=False))
        ws.send(ws_frame(OP_CONT, probe[half:]))
        pong = ws.frame()
        assert pong not in (None, CLOSED) and pong[1] == OP_PONG and \
            pong[2] == b"between", f"expected the pong first, got {pong!r}"
        assert answered(svc, ws)
    finally:
        ws.close()


def test_fragments_delivered_slowly(svc):
    probe = svc.ws_probe().encode()
    ws = open_ws(svc)
    try:
        ws.send(ws_frame(OP_TEXT, probe[:1], fin=False))
        for byte in probe[1:-1]:
            time.sleep(0.02)
            ws.send(ws_frame(OP_CONT, bytes([byte]), fin=False))
        ws.send(ws_frame(OP_CONT, probe[-1:]))
        assert answered(svc, ws)
    finally:
        ws.close()


def test_frame_split_across_segments(svc):
    frame = ws_text(svc.ws_probe())
    ws = open_ws(svc)
    try:
        for byte in frame:
            ws.send(bytes([byte]))
            time.sleep(0.003)
        assert answered(svc, ws)
    finally:
        ws.close()


def test_many_messages_in_one_segment(svc):
    ws = open_ws(svc)
    try:
        count = 1 if svc.name == "btcd" and svc.user else 50
        ws.send(ws_text(svc.ws_probe()) * count)
        assert answered(svc, ws), "message 0"
        # Repeats may be json-rpc errors (electrum: version already set).
        for index in range(1, count):
            got = ws.message()
            assert got not in (None, CLOSED) and got[0] == OP_TEXT, \
                f"message {index}: {got!r}"
    finally:
        ws.close()


def test_ping_payload_echoed(svc):
    ws = open_ws(svc)
    try:
        payload = os.urandom(125)
        ws.send(ws_frame(OP_PING, payload))
        frame = ws.frame()
        assert frame not in (None, CLOSED) and frame[1] == OP_PONG and \
            frame[2] == payload, repr(frame)
    finally:
        ws.close()


def test_ping_flood_then_request(svc):
    ws = open_ws(svc)
    try:
        ws.send(ws_frame(OP_PING, b"p") * 5000)
        ws.text(svc.ws_probe())
        pongs, deadline = 0, time.monotonic() + 3 * REACT
        while time.monotonic() < deadline:
            frame = ws.frame(deadline - time.monotonic())
            if frame in (None, CLOSED):
                break
            if frame[1] == OP_PONG:
                pongs += 1
                continue
            assert frame[1] == OP_TEXT and svc.ws_ok(frame[2]), repr(frame)
            break
        else:
            pytest.fail("no reply after the ping flood")
        assert frame not in (None, CLOSED), f"ended after {pongs} pongs"
    finally:
        ws.close()


def test_clean_close_handshake(svc):
    ws = open_ws(svc)
    try:
        ws.send(ws_frame(OP_CLOSE, struct.pack("!H", 1000) + b"bye"))
        frame = ws.frame()
        assert frame == CLOSED or frame[1] == OP_CLOSE, repr(frame)
    finally:
        ws.close()


def test_data_after_close_frame_is_ignored(svc):
    ws = open_ws(svc)
    try:
        ws.send(ws_frame(OP_CLOSE, struct.pack("!H", 1000)) +
                ws_text(svc.ws_probe()) + b"\xff" * 100)
        assert ends(ws)
    finally:
        ws.close()


def test_binary_message_gets_reaction(svc):
    ws = open_ws(svc)
    try:
        ws.send(ws_frame(OP_BINARY, svc.ws_probe().encode()))
        assert ws.outcome() != SILENT
    finally:
        ws.close()


def test_empty_text_message_gets_reaction(svc):
    ws = open_ws(svc)
    try:
        ws.send(ws_text(b""))
        outcome = ws.outcome(WAIT)
        if outcome == SILENT:
            # An empty message may be skipped; the connection must still work.
            ws.text(svc.ws_probe())
            assert answered(svc, ws)
    finally:
        ws.close()


# ─── Abrupt endings ──────────────────────────────────────────────────────────

@pytest.mark.parametrize("cut", ["header", "mid_payload", "mid_fragments"])
def test_reset_mid_message(svc, cut):
    probe = svc.ws_probe().encode()
    for _ in range(10):
        ws = open_ws(svc)
        if cut == "header":
            ws.send(ws_text(probe)[:3])
        elif cut == "mid_payload":
            ws.send(ws_text(probe)[:-2])
        else:
            ws.send(ws_frame(OP_TEXT, probe[:4], fin=False))
        ws.reset()


def test_close_while_response_in_flight(svc):
    for _ in range(30):
        ws = open_ws(svc)
        ws.text(svc.ws_probe())
        ws.reset()


def test_upgrade_churn(svc):
    def work(index):
        for round_ in range(10):
            try:
                ws = RawWs(svc.host, svc.port)
            except (OSError, ConnectionError):
                continue
            if round_ % 3 == 0:
                ws.text(svc.ws_probe())
            (ws.reset if round_ % 2 else ws.close)()

    run_parallel(12, work)


# ─── Readers that stop reading ───────────────────────────────────────────────

def test_requests_without_reading_responses(svc):
    """A client that floods requests and never reads must hit its own
    backlog (and be dropped or throttled), while others are served."""
    if svc.name == "btcd" and svc.user:
        pytest.skip("covered with an authenticated session in the btcd suite")
    ws = open_ws(svc, rcvbuf=4096)
    try:
        burst = ws_text(svc.ws_probe()) * 200
        stream(ws.sock, b"", burst, 32 * 1024 * 1024, stall=1.0)
        start = time.monotonic()
        problem = canary_problem(svc)
        assert problem is None, problem
        assert time.monotonic() - start < REACT
    finally:
        ws.reset()


def canary_problem(svc):
    from robustness import canary_for
    return canary_for(svc)(svc, REACT)
