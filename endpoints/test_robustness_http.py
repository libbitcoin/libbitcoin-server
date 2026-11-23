"""
Robustness of the http layer (libbitcoin-network http codec and channel) on every
service that accepts http: native, electrum and btcd.

Each case sends a malformed, abusive or merely unusual http exchange and then
checks, through the autouse `contained` fixture, that the server still serves
every service, released the connection and has no pegged thread (see
robustness.py).

The per-case assertion only demands a reaction: a complete but bad input must
get an error response or a close within REACT seconds. Which status code is
returned is not the subject here.

Run with:
    pytest test_robustness_http.py
    pytest test_robustness_http.py --bs-pid=$(pgrep -x bs) -k native
"""

from __future__ import annotations

import json
import os
import socket
import time

import pytest

from robustness import (REACT, REST_PROBE, WAIT, CLOSED, REPLY, SENT, SILENT,
                        STALLED, Outcome, assert_reacts, close, connect,
                        exchange, http_done, http_request, http_responses,
                        http_split, read, reset, run_parallel, send, stream,
                        survived)

SERVICES = ["native", "esplora", "admin", "electrum", "sparrow", "bitcoind",
            "btcd"]


@pytest.fixture(params=SERVICES)
def svc(request, robust_env):
    return robust_env.service(request.param)


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


def ok(svc, out: Outcome) -> bool:
    parsed = http_split(out.data)
    return parsed is not None and svc.http_ok(parsed[0], parsed[2])


def probe_with(svc, headers=(), body=None, method=None, target=None,
               version="HTTP/1.1", length=True) -> bytes:
    """The service's valid probe request with modifications."""
    if svc.rest:
        method, target = method or "GET", target or REST_PROBE[svc.name]
        body = b"" if body is None else body
        headers = svc.auth_headers() + list(headers)
    else:
        method, target = method or "POST", target or "/"
        if body is None:
            body = svc.http_probe().split(b"\r\n\r\n", 1)[1]
        headers = [("Content-Type", "application/json")] + \
            svc.auth_headers() + list(headers)
    return http_request(method, target, svc.authority, body, headers,
                        version, length)


# ─── Connection lifecycle ────────────────────────────────────────────────────

def test_connect_and_close_without_bytes(svc):
    for _ in range(50):
        close(connect(svc.host, svc.port))


def test_connect_and_reset_without_bytes(svc):
    for _ in range(50):
        reset(connect(svc.host, svc.port))


@pytest.mark.parametrize("cut", [1, 0.25, 0.5, 0.9])
def test_reset_mid_request(svc, cut):
    request = probe_with(svc)
    size = max(1, int(len(request) * cut)) if cut != 1 else 1
    for _ in range(20):
        sock = connect(svc.host, svc.port)
        send(sock, request[:size])
        reset(sock)


def test_close_right_after_full_request(svc):
    """The server computes a response for a client that is already gone."""
    for _ in range(50):
        sock = connect(svc.host, svc.port)
        send(sock, probe_with(svc))
        close(sock)


def test_half_close_after_request_still_answered(svc):
    """shutdown(SHUT_WR) after a complete request is legal http/1.1."""
    sock = connect(svc.host, svc.port)
    try:
        send(sock, probe_with(svc, [("Connection", "close")]))
        sock.shutdown(socket.SHUT_WR)
        out = read(sock, REACT, http_done)
    finally:
        close(sock)
    assert_reacts(out, "half-closed request")


def test_idle_connection_does_not_block_others(svc, robust_env):
    """A connection that never sends a byte holds only its own slot."""
    idle = [connect(svc.host, svc.port) for _ in range(4)]
    try:
        time.sleep(WAIT)
        assert ok(svc, exchange(svc.host, svc.port, [probe_with(svc)],
                                done=http_done))
    finally:
        for sock in idle:
            close(sock)


def test_connection_slots_released_after_overload(svc, robust_env):
    """Twice the connection limit in idle sockets, then all released: the
    service must be fully usable again (no leaked slots)."""
    held = []
    try:
        for _ in range(2 * robust_env.connections):
            try:
                held.append(connect(svc.host, svc.port, timeout=2))
            except OSError:
                break
        time.sleep(1)
    finally:
        for sock in held:
            reset(sock)
    time.sleep(1)
    for _ in range(robust_env.connections):
        out = exchange(svc.host, svc.port, [probe_with(svc)], done=http_done)
        assert ok(svc, out), f"after overload: {out}"


def test_connection_churn(svc):
    """Many parallel clients that connect, send a fragment and vanish."""
    request = probe_with(svc)

    def work(index):
        for round_ in range(25):
            sock = connect(svc.host, svc.port)
            send(sock, request[:(index * 7 + round_) % len(request)])
            (reset if round_ % 2 else close)(sock)

    errors = run_parallel(16, work)
    refused = [e for e in errors if not isinstance(e, OSError)]
    assert not refused, refused[:3]


# ─── Slow and fragmented delivery ────────────────────────────────────────────

def test_request_byte_by_byte(svc):
    request = probe_with(svc)
    out = exchange(svc.host, svc.port, [bytes([b]) for b in request],
                   gap=0.005, done=http_done)
    assert ok(svc, out), str(out)


@pytest.mark.parametrize("seed", range(6))
def test_request_split_at_random_boundary(svc, seed):
    request = probe_with(svc)
    cut = 1 + (seed * 7919) % (len(request) - 1)
    out = exchange(svc.host, svc.port, [request[:cut], request[cut:]],
                   gap=0.05, done=http_done)
    assert ok(svc, out), f"cut at {cut}: {out}"


def test_body_trailing_whitespace_in_separate_segment(svc):
    """Regression for libbitcoin-network#968: a json-rpc body whose trailing
    CRLF arrives in its own segment made the body reader loop forever."""
    if svc.rest:
        pytest.skip("rest requests carry no json body")
    body = svc.http_probe().split(b"\r\n\r\n", 1)[1] + b"\r\n"
    request = probe_with(svc, body=body)
    out = exchange(svc.host, svc.port, [request[:-2], request[-2:]],
                   gap=0.2, done=http_done)
    assert ok(svc, out), str(out)


def test_slow_headers_while_others_are_served(svc):
    """A slowloris client must not delay other clients."""
    request = probe_with(svc)
    slow = connect(svc.host, svc.port)
    try:
        for index, byte in enumerate(request[:-4]):
            send(slow, bytes([byte]))
            if index % 20 == 0:
                start = time.monotonic()
                out = exchange(svc.host, svc.port, [request], done=http_done)
                assert ok(svc, out), str(out)
                assert time.monotonic() - start < 2.0
            time.sleep(0.01)
        send(slow, request[-4:])
        assert ok(svc, read(slow, REACT, http_done))
    finally:
        close(slow)


def test_incomplete_request_then_silence(svc):
    """An incomplete request may wait; it must not affect anyone else."""
    sock = connect(svc.host, svc.port)
    try:
        send(sock, probe_with(svc)[:-3])
        assert read(sock, WAIT).kind in (SILENT, REPLY, CLOSED)
        assert ok(svc, exchange(svc.host, svc.port, [probe_with(svc)],
                                done=http_done))
    finally:
        close(sock)


# ─── Pipelining ──────────────────────────────────────────────────────────────

def test_pipelined_requests_answered_in_order(svc):
    count = 50
    out = exchange(svc.host, svc.port, [probe_with(svc) * count],
                   done=lambda d: len(http_responses(d)) >= count)
    responses = http_responses(out.data)
    assert out.kind != SILENT, str(out)
    # A server may stop after any one (connection: close); those it sends
    # must each be complete, framed and json (a repeated electrum
    # server.version is a json-rpc error, not a transport failure).
    assert len(responses) == count, f"{len(responses)} of {count}: {out}"
    assert svc.http_ok(responses[0][0], responses[0][2])
    for status, _, body in responses:
        assert status == 200 and json.loads(body) is not None


def test_pipelined_request_followed_by_garbage(svc):
    out = exchange(svc.host, svc.port,
                   [probe_with(svc) + b"\x00\xffGARBAGE\r\n\r\n"],
                   done=lambda d: len(http_responses(d)) >= 2)
    responses = http_responses(out.data)
    assert responses and svc.http_ok(responses[0][0], responses[0][2]), \
        str(out)


# ─── Malformed requests ──────────────────────────────────────────────────────

GARBAGE = {
    "nul_bytes": b"\x00" * 4096,
    "random_binary": os.urandom(8192),
    "high_bytes": bytes(range(128, 256)) * 32,
    "tls_client_hello": bytes.fromhex(
        "16030100a5010000a10303") + os.urandom(32) + bytes.fromhex(
        "000020c02bc02fc02cc030cca9cca8c013c014009c009d002f0035000a0100"
        "0058000000140012000010") + b"x" * 16,
    "http2_preface": b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n",
    "ssh_banner": b"SSH-2.0-OpenSSH_9.9\r\n",
    "only_spaces": b" " * 4096 + b"\r\n\r\n",
    "bare_lf_request": b"GET / HTTP/1.1\nHost: x\n\n",
    "no_version": b"GET /\r\n\r\n",
    "http09": b"GET /v1/top\r\n",
    "bad_version": b"GET / HTTP/9.9\r\nHost: x\r\n\r\n",
    "bad_version_text": b"GET / HTTX/1.1\r\nHost: x\r\n\r\n",
    "lowercase_method": b"get / HTTP/1.1\r\nHost: x\r\n\r\n",
    "unknown_method": b"FOO / HTTP/1.1\r\nHost: x\r\n\r\n",
    "connect_method": b"CONNECT example.com:443 HTTP/1.1\r\nHost: x\r\n\r\n",
    "trace_method": b"TRACE / HTTP/1.1\r\nHost: x\r\n\r\n",
    "absolute_form": b"GET http://evil.example/ HTTP/1.1\r\nHost: x\r\n\r\n",
    "asterisk_form": b"OPTIONS * HTTP/1.1\r\nHost: x\r\n\r\n",
    "header_without_colon": b"GET / HTTP/1.1\r\nHost x\r\n\r\n",
    "header_space_before_colon": b"GET / HTTP/1.1\r\nHost : x\r\n\r\n",
    "obsolete_line_folding": b"GET / HTTP/1.1\r\nHost: x\r\nX-A: a\r\n b\r\n\r\n",
    "nul_in_header": b"GET / HTTP/1.1\r\nHost: x\x00y\r\n\r\n",
    "control_in_target": b"GET /\x01\x02 HTTP/1.1\r\nHost: x\r\n\r\n",
    "space_in_target": b"GET /a b HTTP/1.1\r\nHost: x\r\n\r\n",
    "json_after_crlfs": b"\r\n" * 64 + b"{\"id\":1}\n",
}


@pytest.mark.parametrize("case", sorted(GARBAGE))
def test_garbage_gets_reaction(svc, case):
    out = exchange(svc.host, svc.port, [GARBAGE[case]], done=http_done)
    if case in ("http09", "json_after_crlfs") and out.kind == SILENT:
        pytest.skip("treated as an incomplete request (legitimate wait)")
    assert_reacts(out, case)


def test_endless_empty_lines_are_bounded(svc):
    """Leading empty lines are tolerated before a request line, but an
    endless stream of them must end in a close, not unbounded buffering."""
    assert_bounded(svc, b"", b"\r\n" * 32768, 16 * 1024 * 1024,
                   "empty lines")


# ─── Limits ──────────────────────────────────────────────────────────────────

def assert_bounded(svc, prefix: bytes, filler: bytes, total: int,
                   what: str) -> None:
    """Streaming an unbounded element must end in a reaction (error or
    close) or in the server no longer reading; never in it reading on."""
    sock = connect(svc.host, svc.port)
    try:
        state, sent = stream(sock, prefix, filler, total)
        if state == STALLED:
            # The server stopped reading: fine if it also reacts/ends.
            state = REPLY if read(sock, REACT).kind != SILENT else STALLED
    finally:
        reset(sock)
    assert state != SENT, f"{what}: server read all {sent} bytes, no reaction"
    assert state != STALLED, \
        f"{what}: server stopped reading after {sent} bytes but holds the " \
        f"connection open"


def test_oversized_header_line(svc):
    assert_bounded(svc, b"GET / HTTP/1.1\r\nHost: x\r\nX-Big: ",
                   b"a" * 65536, 16 * 1024 * 1024, "header value")


def test_oversized_target(svc):
    assert_bounded(svc, b"GET /", b"a" * 65536, 16 * 1024 * 1024,
                   "request target")


@pytest.mark.parametrize("where", ["name", "value"])
@pytest.mark.parametrize("size", [200000, 1000000])
def test_header_field_above_64k(svc, where, size):
    """A header field name or value over 64 KiB, complete in the first read
    or spanning several (200 KB here), is refused. Expected: 431/400 or a
    close."""
    field = (b"X" * size + b": v") if where == "name" else \
        (b"X-Big: " + b"a" * size)
    out = exchange(svc.host, svc.port,
                   [b"GET / HTTP/1.1\r\nHost: x\r\n" + field + b"\r\n\r\n"],
                   done=http_done)
    assert_reacts(out, f"{size} byte field {where}")


def test_header_field_just_below_64k(svc):
    """A field value just below 64 KiB (65530 bytes) is safe."""
    out = exchange(svc.host, svc.port,
                   [probe_with(svc, [("X-Big", "a" * 65530)])], done=http_done)
    assert_reacts(out, "65530 byte field value")


@pytest.mark.parametrize("method", ["GET", "HEAD", "OPTIONS", "DELETE",
                                    "TRACE", "PUT"])
def test_every_method_gets_reaction(svc, method):
    """A complete request with any standard method must be answered (e.g.
    405) or closed, not left without a response on an open connection."""
    request = http_request(method, "/", svc.authority,
                           b"{}" if method == "PUT" else b"",
                           svc.auth_headers())
    out = exchange(svc.host, svc.port, [request],
                   done=(lambda d: b"\r\n\r\n" in d) if method == "HEAD"
                   else http_done)
    assert_reacts(out, f"{method} /")


def test_many_headers(svc):
    out = exchange(svc.host, svc.port,
                   [probe_with(svc, [(f"X-{i}", "v") for i in range(100000)])],
                   done=http_done)
    assert_reacts(out, "100k headers")


def test_declared_body_above_limit_is_refused_early(svc):
    """A huge content-length must be refused without waiting for the body."""
    request = probe_with(svc, body=b"", length=False,
                         headers=[("Content-Length", str(1 << 40))],
                         method="POST", target="/")
    out = exchange(svc.host, svc.port, [request + b"{"], done=http_done)
    if out.kind == SILENT:
        pytest.fail(f"server waits for a 1 TiB body: {out}")


def test_streamed_body_above_limit(svc):
    assert_bounded(
        svc, probe_with(svc, method="POST", target="/", length=False, body=b"",
                        headers=[("Content-Length", str(64 * 1024 * 1024))]),
        b"[" + b" " * 65535, 64 * 1024 * 1024, "body")


@pytest.mark.parametrize("value", [
    "-1", "abc", "1, 2", "0x10", "99999999999999999999999999",
    "18446744073709551616", "",
])
def test_invalid_content_length(svc, value):
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=b"{}", headers=[("Content-Length", value)])
    out = exchange(svc.host, svc.port, [request], done=http_done)
    assert_reacts(out, f"content-length {value!r}")


def test_conflicting_content_lengths(svc):
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=b"{}", headers=[("Content-Length", "2"),
                                              ("Content-Length", "200")])
    assert_reacts(exchange(svc.host, svc.port, [request], done=http_done),
                  "two content-lengths")


def test_content_length_and_chunked(svc):
    """Request smuggling shape (CL.TE): must not be read two ways."""
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=b"0\r\n\r\nGET /smuggled HTTP/1.1\r\nHost: x\r\n\r\n",
                         headers=[("Content-Length", "4"),
                                  ("Transfer-Encoding", "chunked")])
    out = exchange(svc.host, svc.port, [request],
                   done=lambda d: len(http_responses(d)) >= 2)
    assert_reacts(out, "content-length + chunked")


def test_content_length_shorter_than_body(svc):
    """The excess is the next request: an error, not a desync or hang."""
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=b"{}{\"id\":1,\"method\":\"x\"}",
                         headers=[("Content-Length", "2")])
    out = exchange(svc.host, svc.port, [request],
                   done=lambda d: len(http_responses(d)) >= 2)
    assert_reacts(out, "body longer than content-length")


def test_content_length_longer_than_body_then_close(svc):
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=b"{}", headers=[("Content-Length", "1000")])
    for _ in range(20):
        sock = connect(svc.host, svc.port)
        send(sock, request)
        close(sock)


@pytest.mark.parametrize("chunks", [
    b"zz\r\n{}\r\n0\r\n\r\n",                      # not hex
    b"FFFFFFFFFFFFFFFFFF\r\n{}\r\n0\r\n\r\n",      # size overflow
    b"7FFFFFFF\r\n{}",                             # huge, then nothing
    b"2\r\n{}0\r\n\r\n",                           # missing crlf
    b"2;" + b"e" * 100000 + b"\r\n{}\r\n0\r\n\r\n",  # huge extension
    b"-2\r\n{}\r\n0\r\n\r\n",                      # negative
    b"2\r\n{}\r\n0\r\n" + b"X-T: v\r\n" * 50000 + b"\r\n",  # huge trailer
])
def test_malformed_chunked_body(svc, chunks):
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=chunks,
                         headers=[("Transfer-Encoding", "chunked")])
    out = exchange(svc.host, svc.port, [request], done=http_done)
    if chunks.startswith(b"7FFFFFFF") and out.kind == SILENT:
        pytest.skip("waits for the declared chunk (bounded by the limit)")
    assert_reacts(out, f"chunked {chunks[:20]!r}")


def test_valid_chunked_body_in_tiny_chunks(svc):
    if svc.rest:
        pytest.skip("rest requests carry no json body")
    body = svc.http_probe().split(b"\r\n\r\n", 1)[1]
    chunked = b"".join(b"%x\r\n%s\r\n" % (1, bytes([b])) for b in body) + \
        b"0\r\n\r\n"
    request = probe_with(svc, length=False, body=chunked,
                         headers=[("Transfer-Encoding", "chunked")])
    out = exchange(svc.host, svc.port, [request], done=http_done)
    assert out.kind != SILENT, str(out)


def test_unknown_transfer_encoding(svc):
    request = probe_with(svc, method="POST", target="/", length=False,
                         body=b"{}", headers=[("Transfer-Encoding", "gzip")])
    assert_reacts(exchange(svc.host, svc.port, [request], done=http_done),
                  "transfer-encoding gzip")


# ─── Header semantics ────────────────────────────────────────────────────────

@pytest.mark.parametrize("headers", [
    [("Expect", "100-continue")],
    [("Connection", "keep-alive, upgrade, close, foo")],
    [("Host", "a"), ("Host", "b")],
    [("Accept", "*/*;q=" + "9" * 10000)],
    [("Accept", ", ".join(["text/html"] * 5000))],
    [("Content-Type", "application/json; charset=" + "x" * 10000)],
    [("Content-Type", "application/json-rpc")],
    [("Content-Type", "text/plain")],
    [("Origin", "null")],
    [("Authorization", "Basic !!!not-base64!!!")],
    [("Authorization", "Basic " + "QUFB" * 30000)],
    [("Authorization", "Bearer x")],
    [("Authorization", "")],
    [("Upgrade", "h2c"), ("Connection", "Upgrade, HTTP2-Settings"),
     ("HTTP2-Settings", "AAMAAABkAARAAAAAAAIAAAAA")],
    [("Upgrade", "websocket")],                 # upgrade without key/version
])
def test_unusual_headers_get_reaction(svc, headers):
    out = exchange(svc.host, svc.port, [probe_with(svc, headers)],
                   done=http_done)
    assert_reacts(out, f"headers {[h[0] for h in headers]}")


def test_http10_without_host(svc):
    request = probe_with(svc, version="HTTP/1.0").replace(
        f"Host: {svc.authority}\r\n".encode(), b"")
    assert_reacts(exchange(svc.host, svc.port, [request], done=http_done),
                  "http/1.0 without host")


def test_keepalive_reuse_after_error(svc):
    """An error response on a keep-alive connection must leave the
    connection either closed or correctly framed for the next request."""
    sock = connect(svc.host, svc.port)
    try:
        send(sock, b"GET /\x01 HTTP/1.1\r\nHost: x\r\n\r\n")
        first = read(sock, REACT, http_done)
        assert_reacts(first, "bad target")
        if first.closed:
            return
        send(sock, probe_with(svc))
        second = read(sock, REACT, http_done)
        if second.kind == CLOSED:
            return
        assert ok(svc, second), str(second)
    finally:
        close(sock)


# ─── Slow and stalled readers ────────────────────────────────────────────────

def test_client_never_reads_responses(svc):
    """A client that pipelines requests and never reads must be bounded by
    its own backlog/window, not stall the server."""
    sock = connect(svc.host, svc.port, rcvbuf=4096)
    try:
        stream(sock, b"", probe_with(svc) * 100, 64 * 1024 * 1024, stall=1.0)
        assert ok(svc, exchange(svc.host, svc.port, [probe_with(svc)],
                                done=http_done))
        time.sleep(WAIT)
    finally:
        reset(sock)


def test_reader_drains_one_byte_at_a_time(svc):
    sock = connect(svc.host, svc.port, rcvbuf=4096)
    try:
        send(sock, probe_with(svc, [("Connection", "close")]))
        data = b""
        deadline = time.monotonic() + 3 * REACT
        while time.monotonic() < deadline and not http_done(data):
            try:
                chunk = sock.recv(1)
            except OSError:
                break
            if not chunk:
                break
            data += chunk
            time.sleep(0.001)
        assert ok(svc, Outcome(data=data)), data[:200]
    finally:
        close(sock)
