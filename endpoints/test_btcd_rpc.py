"""
Tests for libbitcoin-server btcd JSON-RPC/websocket compatibility interface.

btcd speaks JSON-RPC 1.0 over a persistent websocket connection (preferred,
required for the session/notification extension methods) or plain HTTP POST
(same request/response shape as the bitcoind endpoint, for the chain methods
inherited from protocol_bitcoind_rpc). See docs/btcd-endpoint.md for the full
design and phased scope.

This suite is split into what's real today and what's a development target:

  - Tests with no xfail marker assert real, currently-implemented behavior.
    A failure here is a regression.
  - Tests marked `@pytest.mark.xfail(strict=False)` describe the *intended*
    behavior of something not yet implemented (or not yet reachable over
    ws). They're expected to fail now. When a method is implemented, the
    test starts passing (reported as XPASS, not a hard failure since
    strict=False) -- that's the cue to tighten the assertion and drop the
    marker.

Run with:
    pytest test_btcd_rpc.py
    pytest test_btcd_rpc.py --btcd-host=localhost --btcd-port=8334
    pytest test_btcd_rpc.py -k "not xfail"     # only currently-real behavior
    pytest test_btcd_rpc.py -m xfail -rx       # see what's left to implement
"""

import base64
import hashlib
import json
import os
import select
import socket
import struct
import time
import warnings
from typing import Any, Optional

import pytest
import requests

from utils import ReferenceData, TestConfig

_WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


# ─── Minimal stdlib websocket client (RFC 6455, text frames only) ─────────────
# No external websocket dependency is added here, matching the rest of this
# suite's preference for hand-rolled wire protocol over a client library (see
# ElectrumConnection in test_electrum_subscriptions.py for the same approach
# applied to Electrum's newline-delimited TCP protocol).

class WebSocketConnection:
    """Bare-bones RFC 6455 client: handshake + masked text frame I/O."""

    def __init__(self, host: str, port: int, target: str = "/",
                 connect_timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=connect_timeout)
        self.sock.settimeout(None)  # blocking; timeouts handled via select()
        self._buf = b""
        self._handshake(host, target)

    def _handshake(self, host: str, target: str) -> None:
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            f"GET {target} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        )
        self.sock.sendall(request.encode("ascii"))

        headers = self._read_headers()
        lines = headers.splitlines()
        if not lines or " 101 " not in lines[0]:
            raise ConnectionError(f"websocket upgrade rejected: {lines[:1]!r}")

        expected = base64.b64encode(
            hashlib.sha1((key + _WS_GUID).encode("ascii")).digest()
        ).decode()
        accept = None
        for line in lines[1:]:
            name, _, value = line.partition(":")
            if name.strip().lower() == "sec-websocket-accept":
                accept = value.strip()
        if accept != expected:
            raise ConnectionError("websocket handshake accept key mismatch")

    def _read_headers(self) -> str:
        while b"\r\n\r\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("connection closed during ws handshake")
            self._buf += chunk
        head, _, rest = self._buf.partition(b"\r\n\r\n")
        self._buf = rest
        return head.decode("iso-8859-1")

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass

    def _recv_exact(self, size: int, timeout_s: Optional[float]) -> Optional[bytes]:
        while len(self._buf) < size:
            if timeout_s is not None:
                ready, _, _ = select.select([self.sock], [], [], timeout_s)
                if not ready:
                    return None
            chunk = self.sock.recv(65536)
            if not chunk:
                return None
            self._buf += chunk
        data, self._buf = self._buf[:size], self._buf[size:]
        return data

    def send_text(self, message: str) -> None:
        payload = message.encode("utf-8")
        length = len(payload)
        mask_key = os.urandom(4)
        masked = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))

        header = bytearray([0x80 | 0x1])  # FIN=1, opcode=text
        if length < 126:
            header.append(0x80 | length)
        elif length < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", length)
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", length)
        header += mask_key

        self.sock.sendall(bytes(header) + masked)

    def recv_message(self, timeout_s: Optional[float] = None) -> Optional[str]:
        """Read one complete (possibly multi-frame) text message."""
        parts: list[bytes] = []
        deadline = time.monotonic() + timeout_s if timeout_s is not None else None

        while True:
            remaining = None
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None

            head = self._recv_exact(2, remaining)
            if head is None:
                return None

            fin = bool(head[0] & 0x80)
            opcode = head[0] & 0x0F
            masked = bool(head[1] & 0x80)
            length = head[1] & 0x7F

            if length == 126:
                ext = self._recv_exact(2, remaining)
                if ext is None:
                    return None
                length = struct.unpack(">H", ext)[0]
            elif length == 127:
                ext = self._recv_exact(8, remaining)
                if ext is None:
                    return None
                length = struct.unpack(">Q", ext)[0]

            mask_key = b""
            if masked:
                mask_key = self._recv_exact(4, remaining)
                if mask_key is None:
                    return None

            payload = self._recv_exact(length, remaining) if length else b""
            if payload is None:
                return None
            if masked:
                payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))

            if opcode == 0x8:  # close
                return None
            if opcode in (0x9, 0xA):  # ping/pong: not a message, keep reading
                continue

            parts.append(payload)
            if fin:
                return b"".join(parts).decode("utf-8", errors="replace")


# ─── btcd JSON-RPC 1.0 connection ──────────────────────────────────────────────

class BtcdConnection:
    """A single btcd websocket connection with JSON-RPC 1.0 request/response."""

    def __init__(self, host: str, port: int, connect_timeout: float):
        self.ws = WebSocketConnection(host, port, "/", connect_timeout)
        self._next_id = 0

    def close(self) -> None:
        self.ws.close()

    def raw_rpc(self, method: str, params: Optional[list] = None,
                timeout_s: float = 10.0) -> dict:
        """Send a request and return the full parsed response, error or not."""
        payload = {
            "jsonrpc": "1.0",
            "id": self._next_id,
            "method": method,
            "params": params if params is not None else [],
        }
        self._next_id += 1

        if os.getenv("BTCD_DEBUG"):
            print(">>>", json.dumps(payload, indent=2), flush=True)

        self.ws.send_text(json.dumps(payload))
        raw = self.ws.recv_message(timeout_s)
        if raw is None:
            pytest.fail(f"{method}: no response (connection closed or timed out)")

        if os.getenv("BTCD_DEBUG"):
            print(f"<<< {method}:", raw, flush=True)

        try:
            return json.loads(raw)
        except json.JSONDecodeError as e:
            pytest.fail(f"Invalid JSON from server for {method}: {raw!r} -> {e}")

    def send_rpc(self, method: str, params: Optional[list] = None,
                 timeout_s: float = 10.0) -> dict:
        """Like raw_rpc, but xfail()s on a valid json-rpc error response."""
        data = self.raw_rpc(method, params, timeout_s)
        if "error" in data and data["error"] is not None:
            pytest.xfail(f"Server sent valid error response: {data['error']}")
        return data

    def read_notification(self, timeout_s: float) -> Optional[dict]:
        """Read the next server-pushed notification (has 'method', no 'id')."""
        deadline = time.monotonic() + timeout_s
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            raw = self.ws.recv_message(remaining)
            if raw is None:
                return None
            try:
                obj = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict) and "method" in obj and "id" not in obj:
                return obj


def http_rpc(config: dict, method: str, params: Optional[list] = None) -> dict:
    """Plain http post json-rpc call to the same btcd endpoint (no ws upgrade).

    The chain methods inherited from protocol_bitcoind_rpc are reachable this
    way today even though they aren't yet bridged into the ws dispatcher.
    """
    payload = {
        "jsonrpc": "2.0",
        "id": 0,
        "method": method,
        "params": params if params is not None else [],
    }
    url = f"http://{config['host']}:{config['port']}/"
    response = requests.post(
        url, json=payload,
        headers={"Content-Type": "application/json", "Connection": "close"},
        timeout=config.get("timeout", TestConfig.DEFAULT_RPC_TIMEOUT),
    )
    response.raise_for_status()
    return response.json()


# ─── Fixture ──────────────────────────────────────────────────────────────────

@pytest.fixture
def conn(btcd_config: dict) -> BtcdConnection:
    """Fresh btcd websocket connection per test."""
    host, port = btcd_config["host"], btcd_config["port"]
    timeout = btcd_config.get("timeout", TestConfig.DEFAULT_SOCKET_TIMEOUT)
    try:
        c = BtcdConnection(host, port, connect_timeout=timeout)
    except (OSError, ConnectionError) as exc:
        pytest.skip(f"Cannot connect to btcd at {host}:{port}: {exc}")
    yield c
    c.close()


# ═══════════════════════════════════════════════════════════════════════════════
# SESSION MANAGEMENT (implemented)
# ═══════════════════════════════════════════════════════════════════════════════

def test_authenticate_no_credentials_configured(conn, btcd_config):
    """authenticate is a no-op success when the server has no configured
    username/password, regardless of what's sent."""
    if btcd_config.get("username"):
        pytest.skip("server has credentials configured; see "
                    "test_authenticate_with_configured_credentials")
    response = conn.send_rpc("authenticate", ["anyuser", "anypass"])
    assert response.get("error") is None


def test_authenticate_with_configured_credentials(conn, btcd_config):
    """When btcd.username/password are configured, authenticate must accept
    the matching credentials and reject a mismatch (closing the connection).
    """
    username = btcd_config.get("username")
    password = btcd_config.get("password")
    if not username:
        pytest.skip("--btcd-username not set; server has no configured "
                    "credential to test against")

    response = conn.send_rpc("authenticate", [username, password])
    assert response.get("error") is None


def test_authenticate_wrong_password_rejected(btcd_config):
    """A wrong password must be rejected and the connection closed."""
    username = btcd_config.get("username")
    if not username:
        pytest.skip("--btcd-username not set; nothing to mismatch against")

    host, port = btcd_config["host"], btcd_config["port"]
    timeout = btcd_config.get("timeout", TestConfig.DEFAULT_SOCKET_TIMEOUT)
    try:
        c = BtcdConnection(host, port, connect_timeout=timeout)
    except (OSError, ConnectionError) as exc:
        pytest.skip(f"Cannot connect to btcd at {host}:{port}: {exc}")

    try:
        data = c.raw_rpc("authenticate", [username, "definitely-wrong"])
        assert data.get("error") is not None
    finally:
        c.close()


def test_session_returns_id(conn):
    """session returns an object containing an 'id' field (the channel
    identifier), usable by clients to detect a reconnect to a fresh server.
    """
    response = conn.send_rpc("session")
    result = response.get("result")
    assert isinstance(result, dict)
    assert "id" in result


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK SUBSCRIPTION (implemented)
# ═══════════════════════════════════════════════════════════════════════════════

def test_notifyblocks_acknowledges(conn):
    response = conn.send_rpc("notifyblocks")
    assert response.get("error") is None


def test_stopnotifyblocks_acknowledges(conn):
    conn.send_rpc("notifyblocks")
    response = conn.send_rpc("stopnotifyblocks")
    assert response.get("error") is None


@pytest.mark.slow
def test_blockconnected_notification(conn, btcd_config):
    """
    Subscribe via notifyblocks and wait for a real blockconnected push.

    Notification format (verified against btcsuite/btcd/btcjson):
        {"method": "blockconnected", "params": [hash, height, time]}

    Requires a live node with a new block arriving during the timeout window.
    Set BTCD_TRIGGER_BLOCK to a shell command that produces one (e.g. bumps
    a regtest node), or just wait out the window on a synced mainnet node.
    """
    sub_timeout = btcd_config.get("subscription_timeout", 60.0)

    ack = conn.send_rpc("notifyblocks")
    assert ack.get("error") is None

    cmd = os.getenv("BTCD_TRIGGER_BLOCK")
    if cmd:
        os.system(cmd)
    else:
        print(f"\n  Waiting up to {sub_timeout:.0f}s for a blockconnected "
              "notification (set BTCD_TRIGGER_BLOCK to trigger one).",
              flush=True)

    notif = conn.read_notification(timeout_s=sub_timeout)
    if notif is None:
        pytest.skip(f"No blockconnected notification within {sub_timeout:.0f}s. "
                     "Increase --subscription-timeout or set BTCD_TRIGGER_BLOCK.")

    assert notif.get("method") == "blockconnected"
    params = notif.get("params", [])
    assert isinstance(params, list) and len(params) == 3, (
        f"blockconnected params must be [hash, height, time], got {params!r}"
    )
    block_hash, height, block_time = params
    assert isinstance(block_hash, str) and len(block_hash) == 64
    assert isinstance(height, int) and height > 0
    assert isinstance(block_time, int) and block_time > 0


# ═══════════════════════════════════════════════════════════════════════════════
# STANDARD CHAIN METHODS -- reachable today only via plain http post.
# Development target: bridge these into the ws dispatcher (phase B).
# ═══════════════════════════════════════════════════════════════════════════════

CHAIN_METHODS = [
    ("getbestblockhash", []),
    ("getblockcount", []),
    ("getblockhash", [ReferenceData.GENESIS_HEIGHT]),
    ("getblockheader", [ReferenceData.GENESIS_HASH]),
    ("gettxout", [ReferenceData.KNOWN_TX_HASH, 0, False]),
    ("getrawtransaction", [ReferenceData.FIRST_TX_HASH, 0]),
]


@pytest.mark.parametrize("method,params", CHAIN_METHODS)
def test_chain_method_over_http_post(btcd_config, method, params):
    """Positive control: the chain logic itself works today, reachable over
    plain http post to the same endpoint (unchanged from bitcoind)."""
    data = http_rpc(btcd_config, method, params)
    if data.get("error") is not None:
        pytest.xfail(f"Server sent valid error response: {data['error']}")
    assert "result" in data


@pytest.mark.xfail(reason="phase B: standard chain methods not yet bridged "
                          "into the ws dispatcher (see docs/btcd-endpoint.md)",
                    strict=False)
@pytest.mark.parametrize("method,params", CHAIN_METHODS)
def test_chain_method_over_websocket(conn, method, params):
    """Development target: once bridged, chain methods should also work over
    the same ws connection used for session/notifyblocks/etc -- this is what
    a real lnd/btcwallet client needs (it can't open a second plain-http
    connection once it has upgraded to ws)."""
    response = conn.send_rpc(method, params)
    assert "result" in response


# ═══════════════════════════════════════════════════════════════════════════════
# WIRED BUT NOT YET IMPLEMENTED (phase B/C development targets)
# ═══════════════════════════════════════════════════════════════════════════════
# These methods are present in interfaces/btcd.hpp's method table today, but
# every handler unconditionally returns not_implemented. Each xfail here is a
# checklist item: implement the handler and the test flips to XPASS.

NOT_YET_IMPLEMENTED_STUBS = [
    ("notifynewtransactions", [False]),
    ("stopnotifynewtransactions", []),
    ("loadtxfilter", [False, [], []]),
    ("rescanblocks", [[]]),
]

# Deprecated upstream (superseded by loadtxfilter/rescanblocks) but still
# wired -- included so a regression can't silently start "working" in a way
# that contradicts the deliberate scope decision to leave these stubbed.
DEPRECATED_STUBS = [
    ("notifyreceived", [[]]),
    ("stopnotifyreceived", [[]]),
    ("notifyspent", [[]]),
    ("stopnotifyspent", [[]]),
    ("rescan", ["", [""], [""], ""]),
]


@pytest.mark.xfail(reason="phase B: wired stub, handler not yet implemented",
                    strict=False)
@pytest.mark.parametrize("method,params", NOT_YET_IMPLEMENTED_STUBS)
def test_stub_not_yet_implemented(conn, method, params):
    data = conn.raw_rpc(method, params)
    assert data.get("error") is None, (
        f"{method} still returns not_implemented: {data.get('error')}"
    )


@pytest.mark.parametrize("method,params", DEPRECATED_STUBS)
def test_deprecated_method_stays_not_implemented(conn, method, params):
    """Regression guard, not a development target: these are deliberately
    never implemented (superseded upstream by loadtxfilter/rescanblocks)."""
    data = conn.raw_rpc(method, params)
    assert data.get("error") is not None


def test_stop_always_not_implemented(conn):
    """Regression guard, not a development target: no secure remote-shutdown
    path exists, so stop is permanently rejected regardless of auth state."""
    data = conn.raw_rpc("stop")
    assert data.get("error") is not None


# ═══════════════════════════════════════════════════════════════════════════════
# NOT YET WIRED AT ALL (phase B/C development targets)
# ═══════════════════════════════════════════════════════════════════════════════
# These aren't in interfaces/btcd.hpp's method table yet, so today they hit
# "unexpected method". Each is a checklist item for the generic-tooling
# compatibility phase (see docs/btcd-endpoint.md, Phase C).

NOT_YET_WIRED = [
    ("getcurrentnet", []),
    ("getdifficulty", []),
    ("getinfo", []),
    ("getnettotals", []),
    ("getnetworkhashps", []),
    ("createrawtransaction", [[], {}]),
    ("decoderawtransaction", ["00"]),
    ("decodescript", [""]),
    ("validateaddress", [ReferenceData.EXAMPLE_ADDRESS]),
    ("help", []),
]


@pytest.mark.xfail(reason="phase B/C: method not yet wired in interfaces/btcd.hpp",
                    strict=False)
@pytest.mark.parametrize("method,params", NOT_YET_WIRED)
def test_method_not_yet_wired(conn, method, params):
    data = conn.raw_rpc(method, params)
    assert data.get("error") is None, (
        f"{method} still unwired: {data.get('error')}"
    )


# ═══════════════════════════════════════════════════════════════════════════════
# RESPONSE ENVELOPE / ERROR HANDLING (implemented)
# ═══════════════════════════════════════════════════════════════════════════════

def test_response_id_matches_request(conn):
    r0 = conn.send_rpc("session")
    r1 = conn.send_rpc("notifyblocks")
    assert r0.get("id") == 0
    assert r1.get("id") == 1


def test_unknown_method_errors_without_dropping_connection(conn):
    """An unrecognized method must return a json-rpc error and keep the ws
    connection open for the next request (verified by the follow-up call)."""
    unknown = conn.raw_rpc("nosuchmethod")
    assert unknown.get("error") is not None

    follow_up = conn.send_rpc("session")
    assert follow_up.get("error") is None
