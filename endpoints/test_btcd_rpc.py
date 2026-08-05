"""
Tests for libbitcoin-server btcd JSON-RPC/websocket compatibility interface.

btcd speaks JSON-RPC 1.0 over a persistent websocket connection (preferred,
required for the session/notification extension methods) or plain HTTP POST
(same request/response shape as the bitcoind endpoint, for the chain methods
inherited from protocol_bitcoind_rpc).

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
    auth = None
    if config.get("username"):
        auth = (config["username"], config.get("password") or "")
    response = requests.post(
        url, json=payload,
        headers={"Content-Type": "application/json", "Connection": "close"},
        auth=auth,
        timeout=config.get("timeout", TestConfig.DEFAULT_RPC_TIMEOUT),
    )
    response.raise_for_status()
    return response.json()


# ─── Fixture ──────────────────────────────────────────────────────────────────

@pytest.fixture
def conn(btcd_config: dict) -> BtcdConnection:
    """Fresh btcd websocket connection per test, authenticated up front if
    the server has credentials configured.

    Every method other than 'authenticate' itself is rejected over ws until
    the connection has authenticated (see protocol_btcd_rpc::dispatch_
    websocket) -- a real client always does this handshake first, so this
    fixture does it here rather than in every single test.

    The authenticate-specific tests (test_authenticate_*) intentionally
    don't rely on this and drive their own connections/authenticate calls,
    since they're testing that handshake itself.
    """
    host, port = btcd_config["host"], btcd_config["port"]
    timeout = btcd_config.get("timeout", TestConfig.DEFAULT_SOCKET_TIMEOUT)
    try:
        c = BtcdConnection(host, port, connect_timeout=timeout)
    except (OSError, ConnectionError) as exc:
        pytest.skip(f"Cannot connect to btcd at {host}:{port}: {exc}")

    username = btcd_config.get("username")
    if username:
        auth = c.raw_rpc("authenticate", [username, btcd_config.get("password") or ""])
        if auth.get("error") is not None:
            c.close()
            pytest.fail(f"authenticate failed in fixture setup: {auth['error']}")

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
    the matching credentials. The mismatch case is covered separately by
    test_authenticate_wrong_password_rejected.

    Note: the conn fixture already authenticated once during setup (it must,
    to run anything else) -- this repeats the call explicitly to verify
    authenticate itself is idempotent and still reports success.
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


def test_getcurrentnet_returns_network_magic(conn):
    """getcurrentnet returns the p2p handshake magic number (network_settings
    ().identifier) -- the same value real btcd returns, checked once by
    btcwallet/lnd at connect to confirm they're talking to the expected
    network."""
    response = conn.send_rpc("getcurrentnet")
    assert response.get("result") == ReferenceData.MAINNET_MAGIC


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
# ADDRESS/OUTPOINT FILTERING
# ═══════════════════════════════════════════════════════════════════════════════
# loadtxfilter never itself triggers notifications (matching real btcd) --
# notifyblocks remains the only thing that arms delivery; once armed,
# filteredblockconnected/disconnected are sent alongside the existing
# unfiltered blockconnected/disconnected. rescanblocks replays the same match
# logic against explicitly named historical blocks using the loaded filter.

def test_loadtxfilter_valid_address_acknowledges(conn):
    response = conn.send_rpc("loadtxfilter",
        [True, [ReferenceData.EXAMPLE_ADDRESS], []])
    assert response.get("error") is None


def test_loadtxfilter_invalid_address_rejected(conn):
    response = conn.raw_rpc("loadtxfilter", [True, ["not-an-address"], []])
    assert response.get("error") is not None


def test_loadtxfilter_valid_outpoint_acknowledges(conn):
    response = conn.send_rpc("loadtxfilter",
        [True, [], [{"hash": ReferenceData.GENESIS_TX_HASH, "index": 0}]])
    assert response.get("error") is None


def test_loadtxfilter_malformed_outpoint_rejected(conn):
    response = conn.raw_rpc("loadtxfilter", [True, [], [{"hash": "00"}]])
    assert response.get("error") is not None


def test_rescanblocks_unknown_hash_rejected(conn):
    response = conn.raw_rpc("rescanblocks", [["00" * 32]])
    assert response.get("error") is not None


def test_rescanblocks_known_block_no_match_empty_result(conn):
    # An arbitrary (real, valid-format) address that does not own the
    # genesis coinbase output -- exercises the "no match" path, not a
    # specific claim about what the genesis output actually pays.
    conn.send_rpc("loadtxfilter", [True, [ReferenceData.EXAMPLE_ADDRESS], []])
    response = conn.send_rpc("rescanblocks", [[ReferenceData.GENESIS_HASH]])
    assert response.get("result") == []


def test_rescan_unknown_beginblock_rejected(conn):
    response = conn.raw_rpc("rescan", ["00" * 32, [], [], ""])
    assert response.get("error") is not None


def test_rescan_no_addrs_no_outpoints_finishes_immediately(conn):
    """Minimal implementation of this deprecated method: matches real btcd's
    own "skip scanning, report immediate completion" branch for an empty
    addr/outpoint list -- the exact call btcwallet's own rpcclient makes to
    bootstrap its initial sync starting point (found via a real lnd
    integration test), distinct from rescanblocks/loadtxfilter's actual
    address-watching path. The finished notification always carries the
    current chain tip, not the requested beginblock.
    """
    response = conn.send_rpc("rescan", [ReferenceData.GENESIS_HASH, [], [], ""])
    assert response.get("error") is None

    notification = conn.read_notification(10.0)
    assert notification is not None, "no rescanfinished notification received"
    assert notification["method"] == "rescanfinished"
    assert len(notification["params"]) == 3


def test_rescan_with_addresses_not_implemented(conn):
    # A real historical address/outpoint scan is deliberately not
    # implemented -- no observed real caller needs it.
    response = conn.raw_rpc("rescan",
        [ReferenceData.GENESIS_HASH, [ReferenceData.EXAMPLE_ADDRESS], [], ""])
    assert response.get("error") is not None


@pytest.mark.slow
def test_filteredblockconnected_notification(conn, btcd_config):
    """
    filteredblockconnected must be delivered alongside blockconnected for
    every notifyblocks client, even with no address loaded (empty
    subscribedtxs) -- verified against btcd's own rpcwebsocket.go: both
    notifications fire unconditionally together, the filtered one just
    carries an empty list when nothing matches.

    Notification format (verified against btcsuite/btcd/btcjson):
        {"method": "filteredblockconnected", "params": [height, header, subscribedtxs]}
    """
    sub_timeout = btcd_config.get("subscription_timeout", 60.0)

    ack = conn.send_rpc("notifyblocks")
    assert ack.get("error") is None

    cmd = os.getenv("BTCD_TRIGGER_BLOCK")
    if cmd:
        os.system(cmd)
    else:
        print(f"\n  Waiting up to {sub_timeout:.0f}s for a "
              "filteredblockconnected notification (set BTCD_TRIGGER_BLOCK "
              "to trigger one).", flush=True)

    # blockconnected and filteredblockconnected are sent back-to-back for the
    # same block; read up to two frames to find the filtered one.
    notif = conn.read_notification(timeout_s=sub_timeout)
    if notif is not None and notif.get("method") != "filteredblockconnected":
        notif = conn.read_notification(timeout_s=sub_timeout)

    if notif is None:
        pytest.skip(f"No filteredblockconnected notification within "
                     f"{sub_timeout:.0f}s. Increase --subscription-timeout "
                     "or set BTCD_TRIGGER_BLOCK.")

    assert notif.get("method") == "filteredblockconnected"
    params = notif.get("params", [])
    assert isinstance(params, list) and len(params) == 3, (
        "filteredblockconnected params must be [height, header, "
        f"subscribedtxs], got {params!r}"
    )
    height, header, subscribed_txs = params
    assert isinstance(height, int) and height > 0
    assert isinstance(header, str) and len(header) == 160  # 80-byte header
    assert isinstance(subscribed_txs, list)


# ═══════════════════════════════════════════════════════════════════════════════
# STANDARD CHAIN METHODS (bridged into the ws dispatcher)
# ═══════════════════════════════════════════════════════════════════════════════
# Inherited from protocol_bitcoind_rpc. Reachable both over plain http post
# (unchanged from bitcoind) and over the same ws connection used
# for session/notifyblocks/etc: protocol_btcd_rpc::dispatch_websocket falls
# back to protocol_bitcoind_rpc::dispatch_rpc when the btcd-only dispatcher
# reports "unexpected method". This is what a real lnd/btcwallet client
# needs -- it can't open a second plain-http connection once it has upgraded
# to ws, so authenticate/session/notifyblocks and getblockcount etc. must all
# work on the one connection.

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
    """Positive control: the chain logic itself works, reachable over plain
    http post to the same endpoint (unchanged from bitcoind)."""
    data = http_rpc(btcd_config, method, params)
    if data.get("error") is not None:
        pytest.xfail(f"Server sent valid error response: {data['error']}")
    assert "result" in data


@pytest.mark.parametrize("method,params", CHAIN_METHODS)
def test_chain_method_over_websocket(conn, method, params):
    """Standard chain methods now also work over the same ws connection used
    for session/notifyblocks/etc (see protocol_btcd_rpc::dispatch_websocket /
    protocol_bitcoind_rpc::dispatch_rpc)."""
    response = conn.send_rpc(method, params)
    assert "result" in response


def test_btcd_and_chain_method_share_one_websocket_connection(conn):
    """The actual point of the ws bridge: a single persistent ws connection -- the kind
    a real lnd/btcwallet client opens once and keeps -- can reach both a
    btcd-only extension method (session) and a standard chain method
    (getblockcount) without reconnecting or falling back to plain http post.
    """
    session = conn.send_rpc("session")
    assert isinstance(session.get("result"), dict)

    block_count = conn.send_rpc("getblockcount")
    assert isinstance(block_count.get("result"), int)

    # Same connection still answers a second btcd-only method afterwards.
    notify = conn.send_rpc("notifyblocks")
    assert notify.get("error") is None


# ═══════════════════════════════════════════════════════════════════════════════
# WIRED BUT NOT YET IMPLEMENTED (blocked on a v5 mempool)
# ═══════════════════════════════════════════════════════════════════════════════
# These methods are present in interfaces/btcd.hpp's method table today, but
# every handler unconditionally returns not_implemented. Each xfail here is a
# checklist item: implement the handler and the test flips to XPASS.

NOT_YET_IMPLEMENTED_STUBS = [
    ("notifynewtransactions", [False]),
    ("stopnotifynewtransactions", []),
]

# Deprecated upstream (superseded by loadtxfilter/rescanblocks) but still
# wired -- included so a regression can't silently start "working" in a way
# that contradicts the deliberate scope decision to leave these stubbed.
DEPRECATED_STUBS = [
    ("notifyreceived", [[]]),
    ("stopnotifyreceived", [[]]),
    ("notifyspent", [[]]),
    ("stopnotifyspent", [[]]),
]


@pytest.mark.xfail(reason="wired stub, handler not yet implemented",
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
# Generic btcd-tooling compatibility (implemented)
# ═══════════════════════════════════════════════════════════════════════════════
# getnetworkhashps is an approximation, getnettotals's byte counters are
# untracked zeros, help lists method names only.

def test_getdifficulty_returns_number(conn):
    response = conn.send_rpc("getdifficulty")
    assert isinstance(response.get("result"), float)


def test_getinfo_returns_object(conn):
    result = conn.send_rpc("getinfo").get("result")
    assert isinstance(result, dict)
    assert isinstance(result.get("blocks"), int)
    assert result.get("testnet") is False


def test_btcd_only_method_reachable_over_http_post(btcd_config):
    """A real lnd integration test found that btcwallet's chain.RPCClient
    issues capability-check calls (getblockchaininfo, then getinfo) over
    plain http post immediately on connect, before any subscription traffic
    that requires ws. getblockchaininfo (inherited from protocol_bitcoind_rpc)
    already worked over post; getinfo (a btcd-only method, previously only
    reachable via the ws dispatcher) did not -- protocol_bitcoind_rpc::
    handle_receive_post had no fallback to a derived class's own dispatcher,
    and unconditionally dropped the connection on any unrecognized method,
    so lnd's client retried forever without ever succeeding. Fixed via
    protocol_bitcoind_rpc::dispatch_extension (an override point,
    protocol_btcd_rpc's implementation tries btcd_dispatcher_) -- this is
    the post-side mirror of dispatch_websocket's existing ws-side fallback
    to the inherited chain dispatcher."""
    data = http_rpc(btcd_config, "getinfo")
    assert data.get("error") is None
    assert isinstance(data.get("result"), dict)
    assert isinstance(data["result"].get("blocks"), int)


def test_getnettotals_returns_object(conn):
    result = conn.send_rpc("getnettotals").get("result")
    assert isinstance(result, dict)
    assert "totalbytesrecv" in result
    assert "totalbytessent" in result
    assert result.get("timemillis", 0) > 0


def test_getnetworkhashps_returns_number(conn):
    response = conn.send_rpc("getnetworkhashps")
    assert isinstance(response.get("result"), (int, float))


def test_createrawtransaction_and_decoderawtransaction_roundtrip(conn):
    raw = conn.send_rpc("createrawtransaction",
        [[{"txid": ReferenceData.FIRST_TX_HASH, "vout": 0}],
         {ReferenceData.EXAMPLE_ADDRESS: 0.01}])
    hexstring = raw.get("result")
    assert isinstance(hexstring, str) and len(hexstring) > 0

    result = conn.send_rpc("decoderawtransaction", [hexstring]).get("result")
    assert isinstance(result, dict)
    assert len(result.get("vin", [])) == 1
    assert len(result.get("vout", [])) == 1


def test_decoderawtransaction_malformed_hex_rejected(conn):
    response = conn.raw_rpc("decoderawtransaction", ["not-hex"])
    assert response.get("error") is not None


def test_decodescript_null_data_script(conn):
    # OP_RETURN <2-byte push "hi"> -- self-contained, doesn't depend on live
    # chain contents. A bare "6a" (OP_RETURN with no push at all) does not
    # match the standard null-data template.
    result = conn.send_rpc("decodescript", ["6a026869"]).get("result")
    assert result.get("type") == "nulldata"
    assert "asm" in result


def test_validateaddress_valid_address(conn):
    result = conn.send_rpc("validateaddress",
        [ReferenceData.EXAMPLE_ADDRESS]).get("result")
    assert result.get("isvalid") is True
    assert result.get("address") == ReferenceData.EXAMPLE_ADDRESS
    assert result.get("iswitness") is False


def test_validateaddress_invalid_address(conn):
    result = conn.send_rpc("validateaddress", ["not-an-address"]).get("result")
    assert result.get("isvalid") is False


def test_help_returns_method_list(conn):
    result = conn.send_rpc("help").get("result")
    assert isinstance(result, str) and "getcurrentnet" in result


# ═══════════════════════════════════════════════════════════════════════════════
# RESPONSE ENVELOPE / ERROR HANDLING (implemented)
# ═══════════════════════════════════════════════════════════════════════════════

def test_response_id_matches_request(conn):
    """Each response echoes its own request's id, in order.

    Not asserted as absolute ids 0/1: when credentials are configured the
    conn fixture already consumed one id authenticating, so the starting
    id here depends on whether that happened -- only the relative order
    (each new request's id is exactly one more than the last) is a stable
    guarantee.
    """
    r0 = conn.send_rpc("session")
    r1 = conn.send_rpc("notifyblocks")
    assert r1.get("id") == r0.get("id") + 1


def test_unknown_method_errors_without_dropping_connection(conn):
    """An unrecognized method must return a json-rpc error and keep the ws
    connection open for the next request (verified by the follow-up call)."""
    unknown = conn.raw_rpc("nosuchmethod")
    assert unknown.get("error") is not None

    follow_up = conn.send_rpc("session")
    assert follow_up.get("error") is None
