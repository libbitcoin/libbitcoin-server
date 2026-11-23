"""
Robustness of the electrum services (electrum, sparrow) on their main
transport: newline-delimited json-rpc on a raw tcp connection (the channel
downgrades from http on the first '{' or '[').

Covers malformed and hostile json (depth, size, encoding, types), protocol
misuse (no or repeated server.version, notifications, batches), framing
abuse (missing or endless lines, byte-wise delivery, pipelining) and clients
that subscribe a lot or stop reading.

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_electrum.py
    pytest test_robustness_electrum.py --bs-pid=$(pgrep -x bs)
"""

from __future__ import annotations

import hashlib
import json
import os
import time

import pytest

from robustness import (REACT, WAIT, CLOSED, ENVELOPES, MALFORMED, REPLY, SENT,
                        SILENT, STALLED, RawWs, close, connect, read, reset,
                        run_parallel, send, stream, survived, ws_text)


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


@pytest.fixture(params=["electrum", "sparrow"])
def svc(request, robust_env):
    return robust_env.service(request.param)


def line(obj) -> bytes:
    return (json.dumps(obj) + "\n").encode()


def request(ident, method, params=None) -> bytes:
    return line({"jsonrpc": "2.0", "id": ident, "method": method,
                 "params": [] if params is None else params})


def lines(data: bytes) -> list:
    found = []
    for raw in data.split(b"\n"):
        if raw.strip():
            try:
                found.append(json.loads(raw))
            except ValueError:
                found.append(raw)
    return found


def lines_done(count):
    return lambda data: data.count(b"\n") >= count


class Session:
    """An electrum tcp session that has completed server.version."""

    def __init__(self, svc, handshake: bool = True, rcvbuf=None):
        self.svc = svc
        self.sock = connect(svc.host, svc.port, rcvbuf=rcvbuf)
        self.buffer = b""
        self.closed = False
        if handshake:
            reply = self.call(0, "server.version",
                              ["robustness", svc.protocol])
            assert isinstance(reply, dict) and "result" in reply, reply

    def send(self, data: bytes) -> bool:
        return send(self.sock, data)

    def next(self, wait: float = REACT):
        """Next parsed line, CLOSED, or None on timeout."""
        if b"\n" not in self.buffer and not self.closed:
            out = read(self.sock, wait, lambda d: b"\n" in self.buffer + d)
            self.buffer += out.data
            self.closed = out.closed
        if b"\n" in self.buffer:
            raw, self.buffer = self.buffer.split(b"\n", 1)
            try:
                return json.loads(raw)
            except ValueError:
                return raw
        return CLOSED if self.closed else None

    def call(self, ident, method, params=None, wait: float = REACT):
        self.send(request(ident, method, params))
        while True:
            reply = self.next(wait)
            if not isinstance(reply, dict) or reply.get("id") == ident or \
                    "id" not in reply:
                if isinstance(reply, dict) and "method" in reply and \
                        "id" not in reply:
                    continue        # a notification
                return reply

    def outcome(self, wait: float = REACT) -> str:
        reply = self.next(wait)
        if reply is None:
            return SILENT
        return CLOSED if reply == CLOSED else REPLY

    def works(self) -> bool:
        """The session still answers (False if the server closed it)."""
        reply = self.call(999999, "server.ping")
        return isinstance(reply, dict) and reply.get("error") is None

    def close(self):
        close(self.sock)

    def reset(self):
        reset(self.sock)


@pytest.fixture
def session(svc):
    s = Session(svc)
    yield s
    s.close()


# ─── Malformed json ──────────────────────────────────────────────────────────

@pytest.mark.parametrize("case", sorted(MALFORMED))
def test_malformed_json_after_handshake(session, case):
    session.send(MALFORMED[case])
    outcome = session.outcome()
    if outcome == SILENT and case == "truncated_object":
        # On a stream a newline is json whitespace: the value is incomplete.
        pytest.skip("incomplete json value on a stream (legitimate wait)")
    assert outcome != SILENT, f"{case}: no reaction"


@pytest.mark.parametrize("case", sorted(MALFORMED))
def test_malformed_json_as_first_message(svc, case):
    s = Session(svc, handshake=False)
    try:
        s.send(MALFORMED[case])
        outcome = s.outcome()
        if outcome == SILENT and not MALFORMED[case].lstrip(
                b"\xef\xbb\xbf").startswith((b"{", b"[")):
            # Not json-shaped: read as the start of an http request.
            pytest.skip("not downgraded to json-rpc; incomplete http")
        if outcome == SILENT and case == "truncated_object":
            pytest.skip("incomplete json value on a stream (legitimate wait)")
        assert outcome != SILENT, f"{case}: no reaction"
    finally:
        s.close()


# ─── Envelope shapes ─────────────────────────────────────────────────────────

@pytest.mark.parametrize("case", sorted(ENVELOPES))
def test_unusual_envelope(session, case):
    session.send(line(ENVELOPES[case]))
    assert session.outcome() != SILENT, f"{case}: no reaction"


def test_notification_gets_no_reply_and_session_continues(session):
    """A request without id is a notification: no reply is due."""
    session.send(line({"jsonrpc": "2.0", "method": "server.ping",
                       "params": []}))
    reply = session.next(WAIT)
    if reply == CLOSED:
        pytest.fail("server closed the session on a notification")
    assert session.works()


@pytest.mark.parametrize("batch", [
    [],
    [1],
    [{}],
    [{"jsonrpc": "2.0", "id": i, "method": "server.ping", "params": []}
     for i in range(1000)],
    [{"jsonrpc": "2.0", "id": 1, "method": "server.ping", "params": []},
     "x", 5, None,
     {"jsonrpc": "2.0", "id": 2, "method": "no.such", "params": []}],
    [[{"jsonrpc": "2.0", "id": 1, "method": "server.ping", "params": []}]],
], ids=["empty", "number", "empty_object", "thousand", "mixed", "nested"])
def test_batch_shapes(session, batch):
    session.send(line(batch))
    assert session.outcome() != SILENT


# ─── Parameter edge values ───────────────────────────────────────────────────

PARAMS = [
    ("blockchain.block.header", [-1]),
    ("blockchain.block.header", [2 ** 32]),
    ("blockchain.block.header", [2 ** 64]),
    ("blockchain.block.header", [1.5]),
    ("blockchain.block.header", ["0"]),
    ("blockchain.block.header", [0, 2 ** 31]),
    ("blockchain.block.header", [10, 5]),
    ("blockchain.block.headers", [0, 2 ** 32]),
    ("blockchain.block.headers", [2 ** 32 - 1, 2 ** 32 - 1]),
    ("blockchain.block.headers", [0, -1]),
    ("blockchain.block.headers", [0, 2016, 2 ** 40]),
    ("blockchain.transaction.get", ["00" * 32, True]),
    ("blockchain.transaction.get", ["zz" * 32]),
    ("blockchain.transaction.get", ["00" * 1000000]),
    ("blockchain.transaction.get_merkle", ["00" * 32, 2 ** 40]),
    ("blockchain.transaction.id_from_pos", [0, 2 ** 31, True]),
    ("blockchain.transaction.id_from_pos", [2 ** 31, 0]),
    ("blockchain.transaction.broadcast", ["00"]),
    ("blockchain.transaction.broadcast", ["ff" * 2000000]),
    ("blockchain.scripthash.get_history", ["00" * 33]),
    ("blockchain.scripthash.get_history", [""]),
    ("blockchain.scripthash.get_balance", [None]),
    ("blockchain.scripthash.listunspent", ["x" * 64]),
    ("blockchain.estimatefee", [-1]),
    ("blockchain.estimatefee", [2 ** 40]),
    ("blockchain.estimatefee", [1, "CONSERVATIVE", "extra"]),
    ("server.version", []),
    ("server.version", ["x", ["9.9", "10.0"]]),
    ("server.version", ["x", "0.1"]),
    ("server.version", [{"a": 1}, []]),
    ("server.version", ["x" * 1000000, "1.4"]),
    ("server.add_peer", [{"features": {"hosts": {"x" * 1000: {}}}}]),
    ("mempool.get_fee_histogram", [1, 2, 3]),
]


@pytest.mark.parametrize("method,params", PARAMS,
                         ids=[f"{m}-{i}" for i, (m, _) in enumerate(PARAMS)])
def test_edge_parameters(session, method, params):
    session.send(request(7, method, params))
    assert session.outcome(2 * REACT) != SILENT, f"{method} {params!r:.80}"


# ─── Session protocol misuse ─────────────────────────────────────────────────

def test_method_before_server_version(svc):
    s = Session(svc, handshake=False)
    try:
        s.send(request(1, "blockchain.headers.subscribe"))
        assert s.outcome() != SILENT
    finally:
        s.close()


def test_server_version_twice(session):
    session.send(request(2, "server.version", ["again", session.svc.protocol]))
    assert session.outcome() != SILENT


def test_server_version_many_times(session):
    session.send(b"".join(request(i, "server.version",
                                  ["again", session.svc.protocol])
                          for i in range(100)))
    assert session.outcome() != SILENT


# ─── Framing ─────────────────────────────────────────────────────────────────

def test_empty_lines_and_crlf_around_request(session):
    session.send(b"\n\n\r\n   \n" + request(5, "server.ping")[:-1] +
                 b"\r\n\n\n")
    reply = session.next()
    assert reply not in (None,), "no reaction"
    if reply == CLOSED:
        pytest.xfail("whitespace-only lines close the session")
    assert isinstance(reply, dict) and reply.get("id") == 5, reply


def test_request_byte_by_byte(session):
    for byte in request(5, "server.ping"):
        session.send(bytes([byte]))
        time.sleep(0.003)
    reply = session.next()
    assert isinstance(reply, dict) and reply.get("id") == 5, reply


def test_request_without_newline_then_silence(svc):
    """No newline: an incomplete request may wait, then the client leaves."""
    s = Session(svc)
    try:
        s.send(request(5, "server.ping")[:-1])
        # A complete json value needs no newline; waiting is also fine.
        reply = s.next(WAIT)
        assert reply is None or reply == CLOSED or \
            (isinstance(reply, dict) and reply.get("id") == 5), reply
    finally:
        s.reset()


def test_endless_line_is_bounded(svc):
    """A line that never ends must be cut at maximum_request."""
    s = Session(svc)
    try:
        prefix = b'{"jsonrpc":"2.0","id":1,"method":"server.ping","params":["'
        state, sent = stream(s.sock, prefix, b"a" * 65536, 64 * 1024 * 1024)
        if state == STALLED:
            state = CLOSED if read(s.sock, REACT).kind != SILENT else STALLED
        assert state in (REPLY, CLOSED), \
            f"no reaction after a {sent} byte line ({state})"
    finally:
        s.reset()


def test_endless_whitespace_is_bounded(svc):
    s = Session(svc)
    try:
        state, sent = stream(s.sock, b"", b" " * 65536, 64 * 1024 * 1024)
        if state == STALLED:
            state = CLOSED if read(s.sock, REACT).kind != SILENT else STALLED
        assert state in (REPLY, CLOSED, STALLED), state
        if state == SENT:
            pytest.fail(f"server consumed {sent} bytes of whitespace")
    finally:
        s.reset()


# ─── Volume ──────────────────────────────────────────────────────────────────

def test_pipelined_requests_answered_in_order(session):
    count = 2000
    session.send(b"".join(request(i, "server.ping") for i in range(count)))
    ids = []
    while len(ids) < count:
        reply = session.next(2 * REACT)
        assert isinstance(reply, dict), f"after {len(ids)} replies: {reply!r}"
        ids.append(reply.get("id"))
    assert ids == list(range(count))


def test_large_responses_never_read(svc):
    """Requests for large responses, never read: the backlog must be bounded
    and the server must stay responsive to everyone else."""
    s = Session(svc, rcvbuf=4096)
    try:
        burst = b"".join(request(i, "blockchain.block.headers", [0, 2016])
                         for i in range(200))
        state, sent = stream(s.sock, b"", burst, 16 * 1024 * 1024, stall=1.0)
        time.sleep(WAIT)
        other = Session(svc)
        try:
            assert other.works()
        finally:
            other.close()
    finally:
        s.reset()


def test_disconnect_during_large_responses(svc):
    for _ in range(20):
        s = Session(svc)
        s.send(b"".join(request(i, "blockchain.block.headers", [0, 2016])
                        for i in range(20)))
        s.reset()


def test_many_scripthash_subscriptions_then_reset(svc):
    s = Session(svc)
    try:
        hashes = [hashlib.sha256(os.urandom(8)).hexdigest()
                  for _ in range(5000)]
        s.send(b"".join(request(i, "blockchain.scripthash.subscribe", [h])
                        for i, h in enumerate(hashes)))
        s.send(request("h", "blockchain.headers.subscribe"))
        got = read(s.sock, REACT)
        assert got.kind != SILENT, "no reply to 5000 subscriptions"
    finally:
        s.reset()


def test_subscribe_unsubscribe_churn(session):
    scripthash = hashlib.sha256(b"robustness").hexdigest()
    payload = b""
    for i in range(500):
        payload += request(2 * i, "blockchain.scripthash.subscribe",
                           [scripthash])
        payload += request(2 * i + 1, "blockchain.scripthash.unsubscribe",
                           [scripthash])
    session.send(payload)
    replies = 0
    while replies < 1000:
        reply = session.next(2 * REACT)
        if not isinstance(reply, dict):
            break
        replies += 1
    assert replies == 1000 or reply == CLOSED, \
        f"{replies} replies, then {reply!r}"


def test_good_clients_served_among_bad_ones(svc):
    """Valid sessions keep working while abusive ones run alongside."""
    def bad(index):
        for _ in range(10):
            s = Session(svc, handshake=index % 2 == 0)
            s.send(list(MALFORMED.values())[index % len(MALFORMED)])
            s.send(b"[" * 4096)
            s.reset()

    def good(index):
        s = Session(svc)
        try:
            for i in range(50):
                reply = s.call(i, "blockchain.headers.subscribe" if i % 2
                               else "server.ping")
                assert isinstance(reply, dict) and reply.get("error") is None, \
                    reply
        finally:
            s.close()

    import threading
    errors = []
    bad_thread = threading.Thread(target=lambda: errors.extend(
        run_parallel(8, bad)))
    bad_thread.start()
    errors += run_parallel(8, good)
    bad_thread.join(120)
    failures = [e for e in errors if isinstance(e, AssertionError)]
    assert not failures, failures[:3]


# ─── Over websocket ──────────────────────────────────────────────────────────

@pytest.mark.parametrize("case", ["truncated_object", "deep_array_nesting",
                                  "invalid_utf8", "two_objects_one_line",
                                  "bare_null"])
def test_malformed_json_over_websocket(svc, case):
    try:
        ws = RawWs(svc.host, svc.port)
    except ConnectionError as exc:
        pytest.skip(str(exc))
    try:
        ws.text(json.dumps({"jsonrpc": "2.0", "id": 0,
                            "method": "server.version",
                            "params": ["robustness", svc.protocol]}))
        ws.message()
        payload = MALFORMED[case].rstrip(b"\n")
        if case == "invalid_utf8":
            # Text frames must be utf-8; that is a ws violation instead.
            ws.send(ws_text(payload))
        else:
            ws.text(payload)
        assert ws.outcome() != SILENT, case
    finally:
        ws.close()
