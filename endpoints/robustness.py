"""
Shared tooling for the robustness suites (test_robustness_*.py).

These suites act as broken, slow, hostile or simply sloppy clients. They do not
check what a server answers to a valid request; the other suites do that. They
check that misbehavior stays contained:

    - the misbehaving connection gets an error reply or is closed, within a
      bounded time, unless the input is legitimately incomplete;
    - every other connection keeps working while and after it happens
      ("canaries": one fresh, valid exchange per service);
    - the server releases the connection (no server-side socket is left in
      ESTABLISHED or CLOSE_WAIT once the client is gone);
    - optionally (--bs-pid, server on this host), no server thread is left
      spinning and the process is still alive.

Everything is hand-rolled on raw sockets (http framing, websocket frames,
newline-delimited json), because the point is to send what well-behaved
client libraries refuse to send.
"""

from __future__ import annotations

import base64
import json
import os
import select
import socket
import struct
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Iterable, Optional

import pytest

# How long a server may take to react to a complete (bad) input.
REACT = 5.0

# How long to watch an incomplete input before calling it a legitimate wait.
WAIT = 2.0


# ─── Outcomes ────────────────────────────────────────────────────────────────

REPLY = "reply"      # server sent bytes (may also have closed afterwards)
CLOSED = "closed"    # server closed (fin or reset) without sending anything
SILENT = "silent"    # nothing sent, connection still open at the deadline


@dataclass
class Outcome:
    data: bytes = b""
    closed: bool = False
    reset: bool = False
    send_failed: bool = False
    elapsed: float = 0.0

    @property
    def kind(self) -> str:
        if self.data:
            return REPLY
        return CLOSED if (self.closed or self.send_failed) else SILENT

    def __str__(self) -> str:
        head = self.data[:160]
        return (f"{self.kind} after {self.elapsed:.2f}s "
                f"(closed={self.closed}, reset={self.reset}, "
                f"send_failed={self.send_failed}, {len(self.data)} bytes: "
                f"{head!r}{'...' if len(self.data) > 160 else ''})")


# ─── Raw sockets ─────────────────────────────────────────────────────────────

def connect(host: str, port: int, timeout: float = REACT,
            rcvbuf: Optional[int] = None) -> socket.socket:
    sock = socket.socket(socket.AF_INET6 if ":" in host else socket.AF_INET,
                         socket.SOCK_STREAM)
    if rcvbuf:
        # Must precede connect to shrink the advertised window.
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
    sock.settimeout(timeout)
    sock.connect((host, port))
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return sock


def reset(sock: socket.socket) -> None:
    """Abort the connection with a tcp reset instead of a fin."""
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                        struct.pack("ii", 1, 0))
    except OSError:
        pass
    close(sock)


def close(sock: socket.socket) -> None:
    try:
        sock.close()
    except OSError:
        pass


def send(sock: socket.socket, data: bytes) -> bool:
    """sendall, False if the server already closed or reset."""
    try:
        sock.sendall(data)
        return True
    except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
        return False
    except socket.timeout:
        # Send window closed: the server stopped reading. Not a failure here.
        return True
    except OSError:
        return False


SENT, STALLED = "sent", "stalled"


def push(sock: socket.socket, data: bytes, stall: float = REACT) -> str:
    """Send data: SENT, CLOSED (server closed or reset) or STALLED (the
    server stopped reading for stall seconds)."""
    view = memoryview(data)
    sock.settimeout(stall)
    try:
        while view:
            view = view[sock.send(view):]
        return SENT
    except socket.timeout:
        return STALLED
    except OSError:
        return CLOSED


def stream(sock: socket.socket, prefix: bytes, filler: bytes, total: int,
           stall: float = REACT) -> tuple:
    """Send prefix, then filler until total bytes are sent, the server
    reacts (sends or closes) or stops reading. Returns (state, sent) with
    state REPLY, CLOSED, STALLED or SENT."""
    sent = 0
    state = push(sock, prefix, stall)
    while state == SENT and sent < total:
        state = push(sock, filler, stall)
        if state == SENT:
            sent += len(filler)
            peek = read(sock, 0.0)
            if peek.kind != SILENT:
                return (REPLY if peek.data else CLOSED), sent
    return state, sent


def read(sock: socket.socket, wait: float = REACT,
         done: Optional[Callable[[bytes], bool]] = None,
         limit: int = 64 * 1024 * 1024) -> Outcome:
    """Read until done(data), close/reset, limit bytes or the deadline."""
    out = Outcome()
    start = time.monotonic()
    deadline = start + wait
    while True:
        remaining = max(0.0, deadline - time.monotonic())
        try:
            readable, _, _ = select.select([sock], [], [], remaining)
        except (OSError, ValueError):
            out.closed = True
            break
        if not readable:
            break
        try:
            chunk = sock.recv(1024 * 1024)
        except (ConnectionResetError, ConnectionAbortedError):
            out.closed = out.reset = True
            break
        except socket.timeout:
            continue
        except OSError:
            out.closed = True
            break
        if not chunk:
            out.closed = True
            break
        out.data += chunk
        if (done and done(out.data)) or len(out.data) >= limit:
            break
    out.elapsed = time.monotonic() - start
    return out


def exchange(host: str, port: int, chunks: Iterable[bytes], gap: float = 0.0,
             wait: float = REACT, done: Optional[Callable[[bytes], bool]] = None,
             abort: bool = False) -> Outcome:
    """Connect, send chunks (gap seconds apart), read, then close or reset."""
    sock = connect(host, port)
    failed = False
    try:
        for index, chunk in enumerate(chunks):
            if index and gap:
                time.sleep(gap)
            if not send(sock, chunk):
                failed = True
                break
        out = read(sock, wait, done)
        out.send_failed = failed
        return out
    finally:
        (reset if abort else close)(sock)


def assert_reacts(out: Outcome, what: str) -> None:
    """A complete bad input must be answered or closed, never left hanging."""
    assert out.kind != SILENT, f"{what}: no reaction within the deadline: {out}"


# ─── http ────────────────────────────────────────────────────────────────────

def http_split(data: bytes):
    """(status, headers, body) of a complete http response, else None."""
    head, sep, rest = data.partition(b"\r\n\r\n")
    if not sep:
        return None
    lines = head.split(b"\r\n")
    parts = lines[0].split(b" ", 2)
    if len(parts) < 2 or not parts[0].startswith(b"HTTP/"):
        return None
    try:
        status = int(parts[1])
    except ValueError:
        return None
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(b":")
        headers[name.strip().lower().decode(errors="replace")] = \
            value.strip().decode(errors="replace")
    if status in (101, 204, 304) or 100 <= status < 200:
        return status, headers, b"", len(head) + 4
    if "content-length" in headers:
        try:
            length = int(headers["content-length"])
        except ValueError:
            return None
        if len(rest) < length:
            return None
        return status, headers, rest[:length], len(head) + 4 + length
    if "chunked" in headers.get("transfer-encoding", "").lower():
        body, offset = b"", 0
        while True:
            line_end = rest.find(b"\r\n", offset)
            if line_end < 0:
                return None
            size_text = rest[offset:line_end].split(b";")[0].strip()
            try:
                size = int(size_text, 16)
            except ValueError:
                return None
            offset = line_end + 2
            if size == 0:
                trailer_end = rest.find(b"\r\n", offset)
                if trailer_end < 0:
                    return None
                return status, headers, body, len(head) + 4 + trailer_end + 2
            if len(rest) < offset + size + 2:
                return None
            body += rest[offset:offset + size]
            offset += size + 2
    return None   # delimited by close


def http_responses(data: bytes) -> list:
    """All complete http responses in data, in order."""
    found = []
    while data:
        parsed = http_split(data)
        if parsed is None:
            break
        status, headers, body, used = parsed
        found.append((status, headers, body))
        data = data[used:]
    return found


def http_done(data: bytes) -> bool:
    return http_split(data) is not None


def http_status(out: Outcome) -> Optional[int]:
    parsed = http_split(out.data)
    if parsed is not None:
        return parsed[0]
    line = out.data.split(b"\r\n", 1)[0].split(b" ")
    if len(line) > 1 and line[0].startswith(b"HTTP/") and line[1].isdigit():
        return int(line[1])
    return None


def http_request(method: str, target: str, host: str, body: bytes = b"",
                 headers: Iterable[tuple] = (), version: str = "HTTP/1.1",
                 length: bool = True) -> bytes:
    lines = [f"{method} {target} {version}", f"Host: {host}"]
    lines += [f"{name}: {value}" for name, value in headers]
    if length and (body or method in ("POST", "PUT")):
        lines.append(f"Content-Length: {len(body)}")
    return ("\r\n".join(lines) + "\r\n\r\n").encode() + body


def basic(user: str, password: str) -> tuple:
    token = base64.b64encode(f"{user}:{password}".encode()).decode()
    return ("Authorization", f"Basic {token}")


# ─── websocket (rfc6455, client side) ────────────────────────────────────────

OP_CONT, OP_TEXT, OP_BINARY, OP_CLOSE, OP_PING, OP_PONG = 0, 1, 2, 8, 9, 10


def ws_key() -> str:
    return base64.b64encode(os.urandom(16)).decode()


def ws_upgrade(target: str, host: str, key: Optional[str] = None,
               version: str = "13", headers: Iterable[tuple] = (),
               method: str = "GET") -> bytes:
    fields = [("Upgrade", "websocket"), ("Connection", "Upgrade")]
    if key is not False:
        fields.append(("Sec-WebSocket-Key", key or ws_key()))
    if version is not None:
        fields.append(("Sec-WebSocket-Version", version))
    return http_request(method, target, host, headers=fields + list(headers))


def ws_frame(opcode: int, payload: bytes = b"", fin: bool = True,
             mask: bool = True, rsv: int = 0,
             length: Optional[int] = None) -> bytes:
    """One frame; length overrides the declared payload length."""
    declared = len(payload) if length is None else length
    first = (0x80 if fin else 0) | ((rsv & 7) << 4) | (opcode & 0x0f)
    mask_bit = 0x80 if mask else 0
    if declared < 126:
        head = struct.pack("!BB", first, mask_bit | declared)
    elif declared < 1 << 16:
        head = struct.pack("!BBH", first, mask_bit | 126, declared)
    else:
        head = struct.pack("!BBQ", first, mask_bit | 127, declared)
    if not mask:
        return head + payload
    key = os.urandom(4)
    masked = bytes(b ^ key[i % 4] for i, b in enumerate(payload))
    return head + key + masked


def ws_text(text, **kwargs) -> bytes:
    data = text.encode() if isinstance(text, str) else text
    return ws_frame(OP_TEXT, data, **kwargs)


def ws_parse(data: bytes):
    """(fin, opcode, payload, used) of the first complete server frame."""
    if len(data) < 2:
        return None
    first, second = data[0], data[1]
    length, offset = second & 0x7f, 2
    if length == 126:
        if len(data) < 4:
            return None
        length, offset = struct.unpack("!H", data[2:4])[0], 4
    elif length == 127:
        if len(data) < 10:
            return None
        length, offset = struct.unpack("!Q", data[2:10])[0], 10
    if second & 0x80:
        offset += 4     # server frames must not be masked; tolerate anyway
    if len(data) < offset + length:
        return None
    return bool(first & 0x80), first & 0x0f, data[offset:offset + length], \
        offset + length


class RawWs:
    """A websocket client that sends whatever bytes it is told to."""

    def __init__(self, host: str, port: int, target: str = "/",
                 headers: Iterable[tuple] = (), rcvbuf: Optional[int] = None):
        self.sock = connect(host, port, rcvbuf=rcvbuf)
        send(self.sock, ws_upgrade(target, f"{host}:{port}", headers=headers))
        out = read(self.sock, REACT, lambda d: b"\r\n\r\n" in d)
        head, _, self.buffer = out.data.partition(b"\r\n\r\n")
        self.status = http_status(Outcome(data=head + b"\r\n\r\n"))
        self.closed = out.closed
        if self.status != 101:
            close(self.sock)
            raise ConnectionError(f"websocket upgrade refused: {out}")

    def send(self, data: bytes) -> bool:
        return send(self.sock, data)

    def text(self, text, **kwargs) -> bool:
        return self.send(ws_text(text, **kwargs))

    def frame(self, wait: float = REACT):
        """Next frame as (fin, opcode, payload), CLOSED, or None on timeout."""
        deadline = time.monotonic() + wait
        while True:
            parsed = ws_parse(self.buffer)
            if parsed:
                fin, opcode, payload, used = parsed
                self.buffer = self.buffer[used:]
                return fin, opcode, payload
            if self.closed:
                return CLOSED
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            out = read(self.sock, remaining, lambda d: ws_parse(
                self.buffer + d) is not None)
            self.buffer += out.data
            if out.closed:
                self.closed = True
            elif not out.data:
                return None

    def message(self, wait: float = REACT):
        """Next data message as (opcode, payload): reassembles fragments,
        skips pongs, returns (OP_CLOSE, payload) on a close frame, CLOSED if
        the tcp connection ends, None on timeout."""
        deadline = time.monotonic() + wait
        parts, opcode = [], None
        while True:
            frame = self.frame(max(0.0, deadline - time.monotonic()))
            if frame is None or frame == CLOSED:
                return frame
            fin, op, payload = frame
            if op == OP_PONG:
                continue
            if op == OP_PING:
                self.send(ws_frame(OP_PONG, payload))
                continue
            if op == OP_CLOSE:
                return OP_CLOSE, payload
            if op != OP_CONT:
                opcode = op
            parts.append(payload)
            if fin:
                return opcode, b"".join(parts)

    def outcome(self, wait: float = REACT) -> str:
        """REPLY (data message), CLOSED (close frame or tcp end) or SILENT."""
        got = self.message(wait)
        if got is None:
            return SILENT
        if got == CLOSED or got[0] == OP_CLOSE:
            return CLOSED
        return REPLY

    def drained_closed(self, wait: float = REACT) -> bool:
        """True if the server ends the connection (close frame or tcp end)
        within wait, discarding data messages on the way."""
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            got = self.message(deadline - time.monotonic())
            if got is None:
                return False
            if got == CLOSED or got[0] == OP_CLOSE:
                return True
        return False

    def close(self) -> None:
        close(self.sock)

    def reset(self) -> None:
        reset(self.sock)


# ─── Services ────────────────────────────────────────────────────────────────

# Service families: rest targets (GET and ws text targets), electrum json-rpc
# (server.version handshake) and bitcoind json-rpc (bitcoind, btcd).
REST = ("native", "esplora", "admin")
ELECTRUM = ("electrum", "sparrow")
BITCOIND = ("bitcoind", "btcd")

# A cheap, valid target per rest service.
REST_PROBE = {
    "native": "/v1/top?format=json",
    "esplora": "/blocks/tip/height",
    "admin": "/v1/event/subscribe?format=json",
}


@dataclass
class Service:
    """How to say one valid thing to a service, on each transport."""
    name: str
    host: str
    port: int
    user: Optional[str] = None
    password: Optional[str] = None
    protocol: list = field(default_factory=lambda: ["1.4", "1.6"])

    @property
    def authority(self) -> str:
        return f"{self.host}:{self.port}"

    @property
    def rest(self) -> bool:
        return self.name in REST

    @property
    def electrum(self) -> bool:
        return self.name in ELECTRUM

    def auth_headers(self) -> list:
        return [basic(self.user, self.password)] if self.user else []

    # A complete, valid http request and a check of its response.
    def http_probe(self) -> bytes:
        if self.rest:
            return http_request("GET", REST_PROBE[self.name], self.authority,
                                headers=self.auth_headers())
        body = json.dumps(self.rpc_probe(1)).encode()
        return http_request("POST", "/", self.authority, body,
                            [("Content-Type", "application/json")] +
                            self.auth_headers())

    def http_ok(self, status, body: bytes) -> bool:
        return status == 200 and self.payload_ok(body)

    def payload_ok(self, payload: bytes) -> bool:
        """A valid reply to the probe (http body or ws message)."""
        if self.name == "esplora":
            return payload.strip().isdigit()
        try:
            value = json.loads(payload)
        except ValueError:
            return False
        if self.name == "native":
            return isinstance(value, (int, dict, list))
        if self.name == "admin":
            return isinstance(value, dict) and "previous" in value
        return isinstance(value, dict) and value.get("error") is None

    def rpc_probe(self, ident) -> dict:
        if self.electrum:
            return {"jsonrpc": "2.0", "id": ident, "method": "server.version",
                    "params": ["robustness", self.protocol]}
        return {"jsonrpc": "1.0", "id": ident, "method": "getblockcount",
                "params": []}

    # The first message on a websocket and a check of its reply.
    def ws_probe(self) -> str:
        if self.rest:
            return REST_PROBE[self.name]
        if self.name == "btcd" and self.user:
            return json.dumps({"jsonrpc": "1.0", "id": 1,
                               "method": "authenticate",
                               "params": [self.user, self.password]})
        return json.dumps(self.rpc_probe(1))

    def ws_ok(self, payload: bytes) -> bool:
        return self.payload_ok(payload)


# ─── Hostile json-rpc payloads (shared by the json-rpc services) ─────────────

MALFORMED = {
    "truncated_object": b'{"id":1,"method":"server.ping"\n',
    "unbalanced": b'{"id":1]\n',
    "close_brace_only": b"}\n",
    "bare_null": b"null\n",
    "bare_number": b"1\n",
    "bare_string": b'"server.ping"\n',
    "empty_object": b"{}\n",
    "empty_array": b"[]\n",
    "array_of_numbers": b"[1,2,3]\n",
    "two_objects_one_line":
        b'{"id":1,"method":"server.ping","params":[]}'
        b'{"id":2,"method":"server.ping","params":[]}\n',
    "trailing_garbage": b'{"id":1,"method":"server.ping","params":[]} xyz\n',
    "trailing_comma": b'{"id":1,"method":"server.ping","params":[],}\n',
    "single_quotes": b"{'id':1,'method':'server.ping'}\n",
    "nan_literal": b'{"id":NaN,"method":"server.ping","params":[]}\n',
    "infinity": b'{"id":1,"method":"server.ping","params":[Infinity]}\n',
    "huge_exponent": b'{"id":1e999999,"method":"server.ping","params":[]}\n',
    "long_number": b'{"id":' + b"9" * 100000 + b',"method":"server.ping"}\n',
    "invalid_utf8": b'{"id":1,"method":"server.\xff\xfe","params":[]}\n',
    "overlong_utf8": b'{"id":1,"method":"\xc0\xafserver.ping","params":[]}\n',
    "lone_surrogate": b'{"id":1,"method":"server.ping","params":["\\ud800"]}\n',
    "nul_escape": b'{"id":1,"method":"server.ping\\u0000x","params":[]}\n',
    "raw_control_char": b'{"id":1,"method":"server\x01ping","params":[]}\n',
    "bad_escape": b'{"id":1,"method":"server.\\qping","params":[]}\n',
    "comment": b'{"id":1, /* c */ "method":"server.ping","params":[]}\n',
    "duplicate_keys":
        b'{"id":1,"method":"server.ping","method":"server.banner",'
        b'"params":[]}\n',
    "deep_array_nesting": b'{"id":1,"method":"server.ping","params":' +
        b"[" * 100000 + b"]" * 100000 + b"}\n",
    "deep_object_nesting": b'{"id":1,"method":"server.ping","params":[' +
        b'{"a":' * 50000 + b"1" + b"}" * 50000 + b"]}\n",
    "unterminated_deep_nesting": b"[" * 1000000 + b"\n",
    "bom_prefix": b'\xef\xbb\xbf{"id":1,"method":"server.ping","params":[]}\n',
}


ENVELOPES = {
    "no_method": {"jsonrpc": "2.0", "id": 1, "params": []},
    "method_number": {"jsonrpc": "2.0", "id": 1, "method": 5, "params": []},
    "method_null": {"jsonrpc": "2.0", "id": 1, "method": None},
    "method_object": {"jsonrpc": "2.0", "id": 1, "method": {"a": 1}},
    "method_empty": {"jsonrpc": "2.0", "id": 1, "method": ""},
    "method_huge": {"jsonrpc": "2.0", "id": 1, "method": "m" * 1000000},
    "unknown_method": {"jsonrpc": "2.0", "id": 1, "method": "no.such.method"},
    "params_object": {"jsonrpc": "2.0", "id": 1, "method": "server.ping",
                      "params": {"a": 1}},
    "params_string": {"jsonrpc": "2.0", "id": 1, "method": "server.ping",
                      "params": "x"},
    "params_null": {"jsonrpc": "2.0", "id": 1, "method": "server.ping",
                    "params": None},
    "params_huge_array": {"jsonrpc": "2.0", "id": 1, "method": "server.ping",
                          "params": list(range(200000))},
    "id_object": {"jsonrpc": "2.0", "id": {"a": [1]},
                  "method": "server.ping", "params": []},
    "id_array": {"jsonrpc": "2.0", "id": [1, 2], "method": "server.ping",
                 "params": []},
    "id_float": {"jsonrpc": "2.0", "id": 1.5, "method": "server.ping",
                 "params": []},
    "id_negative": {"jsonrpc": "2.0", "id": -1, "method": "server.ping",
                    "params": []},
    "id_bool": {"jsonrpc": "2.0", "id": True, "method": "server.ping",
                "params": []},
    "id_huge_string": {"jsonrpc": "2.0", "id": "i" * 1000000,
                       "method": "server.ping", "params": []},
    "id_beyond_uint64": {"jsonrpc": "2.0", "id": 2 ** 70,
                         "method": "server.ping", "params": []},
    "jsonrpc_1": {"jsonrpc": "1.0", "id": 1, "method": "server.ping",
                  "params": []},
    "jsonrpc_wrong": {"jsonrpc": "3.0", "id": 1, "method": "server.ping",
                      "params": []},
    "jsonrpc_number": {"jsonrpc": 2, "id": 1, "method": "server.ping",
                       "params": []},
    "extra_members": {"jsonrpc": "2.0", "id": 1, "method": "server.ping",
                      "params": [], "x": "y" * 100000},
}


# ─── Canaries ────────────────────────────────────────────────────────────────

def canary_http(service: Service, wait: float = REACT) -> Optional[str]:
    """None if a fresh, valid http exchange works, else what went wrong."""
    try:
        out = exchange(service.host, service.port, [service.http_probe()],
                       wait=wait, done=http_done)
    except OSError as exc:
        return f"{service.name} http: connect failed: {exc}"
    parsed = http_split(out.data)
    if parsed is None or not service.http_ok(parsed[0], parsed[2]):
        return f"{service.name} http: {out}"
    return None


def canary_ws(service: Service, wait: float = REACT) -> Optional[str]:
    try:
        ws = RawWs(service.host, service.port)
    except (OSError, ConnectionError) as exc:
        return f"{service.name} ws: {exc}"
    try:
        ws.text(service.ws_probe())
        got = ws.message(wait)
        if got is None or got == CLOSED or got[0] != OP_TEXT or \
                not service.ws_ok(got[1]):
            return f"{service.name} ws: {got!r}"
        return None
    finally:
        ws.close()


def canary_tcp(service: Service, wait: float = REACT) -> Optional[str]:
    """Newline-delimited json-rpc (the electrum transport)."""
    line = (json.dumps(service.rpc_probe(1)) + "\n").encode()
    try:
        out = exchange(service.host, service.port, [line], wait=wait,
                       done=lambda d: b"\n" in d)
    except OSError as exc:
        return f"{service.name} tcp: connect failed: {exc}"
    try:
        value = json.loads(out.data.split(b"\n", 1)[0])
    except ValueError:
        return f"{service.name} tcp: {out}"
    if not isinstance(value, dict) or value.get("error") is not None:
        return f"{service.name} tcp: {value}"
    return None


def canary_for(service: Service) -> Callable:
    """The main transport of a service: tcp for electrum, ws for btcd (its
    session transport), http for the others."""
    if service.electrum:
        return canary_tcp
    if service.name == "btcd":
        return canary_ws
    return canary_http


def canaries(services: Iterable[Service], wait: float = REACT) -> list:
    """Every service answers a fresh, valid exchange on its main transport."""
    problems = []
    for service in services:
        probe = canary_for(service)
        problem = probe(service, wait)
        if problem:
            problems.append(problem)
    return problems


# ─── Server-side state (same host only) ──────────────────────────────────────

_TCP_STATES = {"01": "ESTABLISHED", "02": "SYN_SENT", "03": "SYN_RECV",
               "04": "FIN_WAIT1", "05": "FIN_WAIT2", "06": "TIME_WAIT",
               "07": "CLOSE", "08": "CLOSE_WAIT", "09": "LAST_ACK",
               "0A": "LISTEN", "0B": "CLOSING"}


_LOOPBACK = ("0100007F", "0000000000000000FFFF00000100007F",
             "00000000000000000000000001000000")


def _own_inodes() -> set:
    inodes = set()
    for fd in os.listdir("/proc/self/fd"):
        try:
            link = os.readlink(f"/proc/self/fd/{fd}")
        except OSError:
            continue
        if link.startswith("socket:["):
            inodes.add(link[8:-1])
    return inodes


def server_sockets(ports: Iterable[int]) -> dict:
    """{(port, state): count} of server-side sockets on the given ports.

    Connections of other clients (a non-loopback peer, or a loopback peer
    socket owned by another process) are not the tests' and are left out."""
    wanted = set(ports)
    sockets = []
    for table in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            with open(table) as f:
                next(f)
                for line in f:
                    cols = line.split()
                    sockets.append((cols[1], cols[2],
                                    _TCP_STATES.get(cols[3], cols[3]), cols[9]))
        except OSError:
            pass
    clients = {(local, remote): inode for local, remote, state, inode in sockets
               if state != "TIME_WAIT"}
    own = _own_inodes()
    found: dict = {}
    for local, remote, state, _ in sockets:
        port = int(local.rsplit(":", 1)[1], 16)
        if port not in wanted or state in ("LISTEN", "TIME_WAIT"):
            continue
        if remote.rsplit(":", 1)[0] not in _LOOPBACK:
            continue
        peer = clients.get((remote, local))
        if peer not in (None, "0") and peer not in own:
            continue
        found[(port, state)] = found.get((port, state), 0) + 1
    return found


def lingering(ports: Iterable[int], wait: float = 10.0) -> dict:
    """Server sockets still held after wait seconds with no client left.

    ESTABLISHED/CLOSE_WAIT on a service port while no client is connected
    means the server did not release a connection (its slot is lost)."""
    deadline = time.monotonic() + wait
    while True:
        held = {k: v for k, v in server_sockets(ports).items()
                if k[1] in ("ESTABLISHED", "CLOSE_WAIT", "SYN_RECV")}
        if not held or time.monotonic() >= deadline:
            return held
        time.sleep(0.25)


class Process:
    """The server process, when it runs on this host (--bs-pid)."""

    def __init__(self, pid: int):
        self.pid = pid
        self.hz = os.sysconf("SC_CLK_TCK")

    def alive(self) -> bool:
        try:
            with open(f"/proc/{self.pid}/stat") as f:
                return f.read().split(")")[-1].split()[0] != "Z"
        except OSError:
            return False

    def _ticks(self) -> dict:
        ticks = {}
        try:
            tids = os.listdir(f"/proc/{self.pid}/task")
        except OSError:
            return ticks
        for tid in tids:
            try:
                with open(f"/proc/{self.pid}/task/{tid}/stat") as f:
                    fields = f.read().rsplit(")", 1)[1].split()
                ticks[tid] = int(fields[11]) + int(fields[12])
            except (OSError, IndexError, ValueError):
                pass
        return ticks

    def busy_threads(self, window: float = 3.0,
                     threshold: float = 0.9) -> list:
        """Threads that used more than threshold of a core over window."""
        before = self._ticks()
        time.sleep(window)
        after = self._ticks()
        busy = []
        for tid, ticks in after.items():
            used = (ticks - before.get(tid, ticks)) / self.hz / window
            if used >= threshold:
                busy.append((tid, round(used, 2)))
        return busy

    def rss_mb(self) -> float:
        try:
            with open(f"/proc/{self.pid}/status") as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        return int(line.split()[1]) / 1024
        except OSError:
            pass
        return 0.0


# ─── The invariant checked after every robustness test ──────────────────────

@dataclass
class Environment:
    services: list
    local: bool
    process: Optional[Process]
    spin_check: bool
    connections: int

    def service(self, name: str) -> Service:
        for service in self.services:
            if service.name == name:
                return service
        pytest.skip(f"{name} endpoint not configured for robustness tests")

    def ports(self) -> list:
        return [service.port for service in self.services]


def survived(env: Environment) -> None:
    """Fail the current test if the server did not contain the damage."""
    problems = []

    if env.process and not env.process.alive():
        pytest.fail(f"server process {env.process.pid} is gone")

    problems += canaries(env.services)

    if env.local:
        held = lingering(env.ports())
        if held:
            problems.append(f"connections not released by the server: {held}")

    if env.process and env.spin_check:
        busy = env.process.busy_threads()
        if busy:
            # Confirm: a legitimately busy thread (e.g. a validation burst)
            # rarely stays pegged for a second window.
            again = {tid for tid, _ in env.process.busy_threads()}
            pegged = [b for b in busy if b[0] in again]
            if pegged:
                problems.append(f"server threads pegged after the test "
                                f"(tid, cores): {pegged}")

    if problems:
        pytest.fail("server did not survive:\n  " + "\n  ".join(problems))


def run_parallel(count: int, work: Callable[[int], None]) -> list:
    """Run work(i) in count threads, return the exceptions raised."""
    errors: list = []
    lock = threading.Lock()

    def wrapped(index: int) -> None:
        try:
            work(index)
        except Exception as exc:          # noqa: BLE001 - collected
            with lock:
                errors.append(exc)

    threads = [threading.Thread(target=wrapped, args=(i,), daemon=True)
               for i in range(count)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(120)
    return errors
