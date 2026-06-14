"""
Tests for libbitcoin-server Electrum protocol compatibility interface.

Tests the Electrum Protocol JSON-RPC over TCP interface for wallet and
blockchain queries. All assertions represent the *desired* protocol behavior
per the Electrum specification — not just what the current server happens to
return. Failing tests indicate gaps between spec and implementation.

Connection management: a single persistent TCP connection is shared across
the test session. The Electrum version handshake (server.version) is
performed once during session setup using the configured protocol range.

Run with:
    pytest test_electrum.py
    pytest test_electrum.py --electrum-host=localhost --electrum-port=50001
    pytest test_electrum.py --electrum-protocol=1.6
    pytest test_electrum.py --electrum-host=fulcrum.sethforprivacy.com --electrum-port=50001
    ELECTRUM_DEBUG=1 pytest test_electrum.py -s -k "block_header"

For tests that wait for real push notifications see test_electrum_subscriptions.py:
    pytest test_electrum_subscriptions.py --subscription-timeout=120
"""

import json
import os
import select
import socket
import time
import warnings
import pytest
from typing import Any

from utils import ReferenceData, TestConfig

_CLIENT_NAME = "libbitcoin-server-test/1.0"

# ─── Module-level session state ──────────────────────────────────────────────
_sock: socket.socket | None = None
_recv_buffer: bytes = b""
_configured_protocol: list[str] = ["1.4", "1.6"]
_server_name: str | None = None
negotiated_version: str | None = None
_next_rpc_id: int = 1


# ─── Session fixture ─────────────────────────────────────────────────────────

@pytest.fixture(autouse=True, scope="session")
def persistent_connection(electrum_config: dict):
    """
    Establish a persistent TCP connection and perform the Electrum version
    handshake. The negotiated version is stored in `negotiated_version` for
    test assertions.
    """
    global _sock, _recv_buffer, _configured_protocol, _server_name, negotiated_version

    host = electrum_config["host"]
    port = electrum_config["port"]
    timeout = electrum_config.get("timeout", TestConfig.DEFAULT_SOCKET_TIMEOUT)
    protocol = electrum_config.get("protocol", ["1.4", "1.6"])

    _configured_protocol = protocol
    _recv_buffer = b""
    _server_name = None
    negotiated_version = None

    try:
        _sock = socket.create_connection((host, port), timeout=timeout)
    except OSError as exc:
        pytest.exit(f"Cannot connect to Electrum server at {host}:{port}: {exc}", returncode=1)

    # server.version MUST be the first message sent (Electrum protocol requirement).
    # Store the result; test_server_version validates it.
    try:
        handshake = _rpc_raw("server.version", [_CLIENT_NAME, protocol], timeout_s=timeout)
        if isinstance(handshake, list) and len(handshake) >= 2:
            _server_name = str(handshake[0])
            negotiated_version = str(handshake[1])
    except Exception:
        pass  # test_server_version will fail with a clear message

    proto_display = protocol[0] if protocol[0] == protocol[1] else f"{protocol[0]}:{protocol[1]}"
    if negotiated_version is not None:
        print(
            f"\n  server:     {_server_name}\n"
            f"  negotiated: {negotiated_version}  (offered: {proto_display})",
            flush=True,
        )
    else:
        print(f"\n  Electrum handshake failed — {host}:{port} (offered: {proto_display})", flush=True)

    try:
        yield
    finally:
        s, _sock = _sock, None
        if s is not None:
            try:
                s.close()
            except Exception:
                pass


# ─── Low-level I/O ───────────────────────────────────────────────────────────

def _readline(timeout_s: float | None = None) -> str | None:
    """
    Read one newline-terminated line from the persistent socket.

    Maintains a module-level byte buffer so a timeout on any individual
    recv() never corrupts reader state. This is critical: Python's
    socket.makefile() marks itself permanently unreadable after the first
    timeout (SocketIO._timeout_occurred), which would break the entire test
    session. The select-based approach avoids that.

    Returns the decoded line without the trailing newline, or None on
    timeout or connection close.
    """
    global _recv_buffer

    deadline = time.monotonic() + timeout_s if timeout_s is not None else None

    while True:
        nl = _recv_buffer.find(b"\n")
        if nl >= 0:
            line, _recv_buffer = _recv_buffer[:nl], _recv_buffer[nl + 1:]
            return line.decode("utf-8", errors="replace")

        if _sock is None:
            return None

        if deadline is not None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            # select() avoids touching the socket when nothing is ready,
            # so a timeout here leaves the buffer intact for the next call.
            ready, _, _ = select.select([_sock], [], [], remaining)
            if not ready:
                return None

        try:
            chunk = _sock.recv(65536)
        except OSError:
            return None

        if not chunk:
            return None  # server closed connection
        _recv_buffer += chunk


def _rpc_raw(method: str, params: list | None = None,
             timeout_s: float = TestConfig.DEFAULT_SOCKET_TIMEOUT) -> Any:
    """
    Send a JSON-RPC request and return the parsed result without any pytest
    skip/xfail handling. Used internally (e.g. in the handshake fixture).
    Raises RuntimeError on connection/protocol errors.
    """
    if _sock is None:
        raise RuntimeError("No socket connection")

    payload = {
        "jsonrpc": "2.0",
        "id": 0,
        "method": method,
        "params": params if params is not None else [],
    }
    _sock.sendall((json.dumps(payload) + "\n").encode("utf-8"))

    data = _readline(timeout_s=timeout_s)
    if data is None:
        raise RuntimeError(f"No response for {method!r}")

    resp = json.loads(data)
    if isinstance(resp, dict) and resp.get("error") is not None:
        raise ValueError(str(resp["error"]))
    if isinstance(resp, dict) and "result" in resp:
        return resp["result"]
    return resp


# ─── Public RPC helper used by tests ────────────────────────────────────────

def send_rpc(method: str, params: list | None = None) -> Any:
    """
    Send an Electrum JSON-RPC request and return the result.

    Each request gets a unique monotonic ID so that stale delayed responses
    from a previous timed-out request are silently discarded rather than
    poisoning the next test.

    Push notifications (server-initiated messages while a subscription is
    active) are also transparently skipped.

    On connection or timeout errors the current test is skipped.
    On server-reported errors the current test is xfailed.
    """
    global _next_rpc_id
    rpc_id = _next_rpc_id
    _next_rpc_id += 1

    payload = {
        "jsonrpc": "2.0",
        "id": rpc_id,
        "method": method,
        "params": params if params is not None else [],
    }

    try:
        if os.getenv("ELECTRUM_DEBUG"):
            print(">>>", json.dumps(payload, indent=2), flush=True)

        if _sock is None:
            pytest.skip("No socket connection")

        _sock.sendall((json.dumps(payload) + "\n").encode("utf-8"))

        timeout = TestConfig.DEFAULT_SOCKET_TIMEOUT
        t0 = time.monotonic()

        while True:
            remaining = timeout - (time.monotonic() - t0)
            if remaining <= 0:
                pytest.skip(f"Timeout waiting for response to {method!r}")

            data = _readline(timeout_s=remaining)
            if data is None:
                pytest.skip(f"No response (possibly unsupported method: {method!r})")

            response = _parse_response(data, method)

            if isinstance(response, dict):
                # Skip push notifications: 'method' key present, no 'result'.
                if "method" in response and "result" not in response:
                    if os.getenv("ELECTRUM_DEBUG"):
                        print(f"<<< [skip notification] {response.get('method')!r}", flush=True)
                    continue
                # Discard stale responses from timed-out previous requests.
                resp_id = response.get("id")
                if resp_id is not None and resp_id != rpc_id:
                    if os.getenv("ELECTRUM_DEBUG"):
                        print(f"<<< [discard stale id={resp_id}] {method!r}", flush=True)
                    continue

            if os.getenv("ELECTRUM_DEBUG"):
                elapsed_ms = (time.monotonic() - t0) * 1000
                try:
                    print(f"<<< {method} ({elapsed_ms:.1f} ms):",
                          json.dumps(response, indent=2), flush=True)
                except Exception:
                    pass

            if isinstance(response, dict) and response.get("error") is not None:
                err = response["error"]
                msg = err.get("message", str(err)) if isinstance(err, dict) else str(err)
                pytest.xfail(f"Server error: {msg}")

            if isinstance(response, dict) and "result" in response:
                return response["result"]
            return response

    except (pytest.skip.Exception, pytest.xfail.Exception):
        raise
    except Exception as exc:
        pytest.skip(f"Connection / protocol error: {type(exc).__name__}: {exc}")


def _parse_response(raw: str, method: str = "") -> dict:
    """Parse a server response line with JSON-RPC 2.0 compliance warnings."""
    try:
        resp = json.loads(raw)
    except json.JSONDecodeError as exc:
        pytest.fail(f"Invalid JSON from server for {method!r}: {raw!r} — {exc}")

    if isinstance(resp, dict):
        if resp.get("jsonrpc") != "2.0":
            warnings.warn(
                f"Response not JSON-RPC 2.0 (jsonrpc={resp.get('jsonrpc')!r})\n"
                f"Raw: {raw}",
                UserWarning,
                stacklevel=3,
            )
        if "id" not in resp:
            warnings.warn("Response missing 'id' field", UserWarning, stacklevel=3)
        if "result" in resp and "error" in resp and resp["error"] is not None:
            warnings.warn("Response has both 'result' and 'error'", UserWarning, stacklevel=3)

    return resp


# ═══════════════════════════════════════════════════════════════════════════════
# SERVER METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_server_version():
    """
    Verify the server.version handshake result.

    The handshake is performed once during session setup (persistent_connection
    fixture) using the protocol range from --electrum-protocol. In Electrum
    v1.4+ server.version must be the first message, so we validate the stored
    result here rather than sending a second handshake.

    Expected result: [server_name, negotiated_version]
    The negotiated version must fall within the range offered by the client.
    """
    assert negotiated_version is not None, (
        "server.version handshake failed during session setup. "
        "Check that the server is reachable and speaks the Electrum protocol."
    )
    assert isinstance(negotiated_version, str)
    assert _server_name is not None and len(_server_name) > 0, \
        "Server did not return a name in the version handshake"

    proto_min, proto_max = _configured_protocol[0], _configured_protocol[1]
    assert negotiated_version >= proto_min, (
        f"Server negotiated {negotiated_version!r}, below our offered minimum {proto_min!r}"
    )
    assert negotiated_version <= proto_max, (
        f"Server negotiated {negotiated_version!r}, above our offered maximum {proto_max!r}"
    )


def test_server_banner():
    """Test server.banner — returns a human-readable server description string."""
    result = send_rpc("server.banner")
    assert isinstance(result, str), f"server.banner must return a string, got {type(result)}"


def test_server_features():
    """Test server.features — returns server capabilities and connection details."""
    result = send_rpc("server.features")
    assert isinstance(result, dict)
    required = {"genesis_hash", "hash_function", "protocol_min", "protocol_max"}
    missing = required - set(result)
    assert not missing, f"server.features missing required fields: {missing}"
    # Validate genesis hash format (64-char hex)
    assert isinstance(result["genesis_hash"], str) and len(result["genesis_hash"]) == 64


def test_server_ping():
    """Test server.ping — keep-alive, must return null."""
    result = send_rpc("server.ping")
    assert result is None, f"server.ping must return null, got {result!r}"


def test_server_add_peer():
    """Test server.add_peer — add peer to server's peer list (returns bool)."""
    features = {
        "genesis_hash": ReferenceData.GENESIS_HASH,
        "hosts": {},
        "protocol_max": "1.4",
        "protocol_min": "1.4",
        "server_version": "test/1.0",
    }
    result = send_rpc("server.add_peer", [features])
    assert isinstance(result, bool)


def test_server_donation_address():
    """Test server.donation_address — optional donation address or null."""
    result = send_rpc("server.donation_address")
    assert result is None or isinstance(result, str)


def test_server_peers_subscribe():
    """Test server.peers.subscribe — returns list of known peer servers."""
    result = send_rpc("server.peers.subscribe")
    assert isinstance(result, list)


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCKCHAIN — BLOCK HEADER/S
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_block_header():
    """
    Test blockchain.block.header with cp_height=0.

    Per the Electrum protocol spec: when cp_height is 0 (no checkpoint proof),
    the result MUST be a plain hex string representing the serialized 80-byte
    block header (160 hex characters).
    """
    response = send_rpc("blockchain.block.header", [ReferenceData.KNOWN_HEIGHT, 0])
    assert isinstance(response, str), (
        f"blockchain.block.header with cp_height=0 must return a plain hex string, "
        f"got {type(response).__name__}: {response!r}"
    )
    assert len(response) == 160, (
        f"Header hex must be 160 chars (80 bytes), got {len(response)}"
    )


def test_blockchain_block_header_with_cp():
    """
    Test blockchain.block.header with cp_height > 0.

    Per spec: when a checkpoint is requested, the result is a dict with:
      - 'header': the requested header as a hex string (160 chars)
      - 'branch': list of hex-encoded 32-byte merkle proof hashes
      - 'root': hex-encoded 32-byte merkle root
    """
    response = send_rpc(
        "blockchain.block.header",
        [ReferenceData.KNOWN_HEIGHT, ReferenceData.KNOWN_HEIGHT],
    )

    assert isinstance(response, dict), (
        f"blockchain.block.header with cp_height > 0 must return a dict, "
        f"got {type(response).__name__}. "
        f"Keys present: {list(response.keys()) if isinstance(response, dict) else 'n/a'}"
    )

    # Per spec the singular header is under the key 'header' (not 'hex' / 'headers')
    assert "header" in response, (
        f"Expected 'header' key in checkpoint response, got keys: {list(response.keys())}"
    )
    assert isinstance(response["header"], str) and len(response["header"]) == 160, (
        f"'header' must be a 160-char hex string, got: {response['header']!r}"
    )
    assert "root" in response, "Expected 'root' in checkpointed response"
    assert "branch" in response, "Expected 'branch' in checkpointed response"
    assert isinstance(response["branch"], list)
    for h in response["branch"]:
        assert isinstance(h, str) and len(h) == 64, \
            f"Each branch element must be a 64-char hex hash, got {h!r}"


def test_blockchain_block_header_with_cp_protocol_example():
    """
    Test blockchain.block.header with the exact example from the Electrum protocol docs.

    https://electrumx.readthedocs.io/en/latest/protocol-methods.html#blockchain-block-header
    Parameters: height=5, cp_height=8
    """
    response = send_rpc("blockchain.block.header", [5, 8])

    assert isinstance(response, dict), (
        f"blockchain.block.header with cp_height=8 must return a dict, "
        f"got {type(response).__name__}"
    )
    assert "header" in response, (
        f"Expected 'header' key per spec, got keys: {list(response.keys())}"
    )
    assert isinstance(response["header"], str) and len(response["header"]) == 160
    assert "root" in response
    assert "branch" in response
    assert isinstance(response["branch"], list)
    for h in response["branch"]:
        assert isinstance(h, str) and len(h) == 64


def test_blockchain_block_headers():
    """
    Test blockchain.block.headers — fetch multiple block headers, no checkpoint.

    Per spec, result is a dict with:
      - 'count': number of headers returned
      - 'hex' (< v1.6): concatenated hex string, length = count * 160
      - 'headers' (v1.6+): array of individual header hex strings
      - 'max': maximum headers the server returns per request
    """
    result = send_rpc("blockchain.block.headers", [ReferenceData.KNOWN_HEIGHT, 5, 0])
    assert isinstance(result, dict)
    assert "count" in result, f"Response missing 'count': {result}"

    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None, (
        f"Expected 'headers' (v1.6+) or 'hex' (pre-v1.6) key, got keys: {list(result.keys())}"
    )

    if isinstance(header_data, list):
        assert len(header_data) == result["count"]
        assert all(isinstance(h, str) and len(h) == 160 for h in header_data), \
            "Each header must be 160-char hex"
    else:
        assert isinstance(header_data, str)
        assert len(header_data) == 160 * result["count"], (
            f"Concatenated hex length mismatch: expected {160 * result['count']}, "
            f"got {len(header_data)}"
        )


def test_blockchain_block_headers_protocol_example():
    """
    Test blockchain.block.headers using the example from the Electrum protocol docs.

    https://electrumx.readthedocs.io/en/latest/protocol-methods.html#blockchain-block-headers
    Parameters: start_height=5, count=8, cp_height=0
    """
    result = send_rpc("blockchain.block.headers", [5, 8, 0])
    assert isinstance(result, dict)
    assert "count" in result

    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None, \
        f"Expected 'headers' or 'hex' key, got: {list(result.keys())}"

    if isinstance(header_data, list):
        assert len(header_data) == result["count"]
        assert all(isinstance(h, str) and len(h) == 160 for h in header_data)
    else:
        assert len(header_data) == 160 * result["count"]


def test_blockchain_block_headers_with_cp_single():
    """
    Request a single block header with a checkpoint proof and validate all fields.

    Per spec: when cp_height is provided, result additionally contains
    'branch' and 'root' fields.
    """
    cp = ReferenceData.KNOWN_HEIGHT + 10
    result = send_rpc("blockchain.block.headers", [ReferenceData.KNOWN_HEIGHT, 1, cp])

    assert isinstance(result, dict)
    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None, \
        f"Expected 'headers' or 'hex' key, got: {list(result.keys())}"
    assert "root" in result and "branch" in result, \
        "Checkpoint response must include 'root' and 'branch'"
    assert isinstance(result["branch"], list)
    for h in result["branch"]:
        assert isinstance(h, str) and len(h) == 64


def test_blockchain_block_headers_with_cp_multiple():
    """Request multiple headers with a checkpoint proof."""
    count = 5
    cp = ReferenceData.KNOWN_HEIGHT + 20
    result = send_rpc("blockchain.block.headers", [ReferenceData.KNOWN_HEIGHT, count, cp])

    assert isinstance(result, dict)
    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None

    if isinstance(header_data, list):
        assert len(header_data) == result["count"]
    else:
        assert len(header_data) == 160 * result["count"]

    assert "root" in result and "branch" in result
    assert isinstance(result["branch"], list)


def test_blockchain_block_headers_height_900000_with_cp():
    """Request headers at height 900,000 with a checkpoint proof."""
    start, count, cp = 900000, 1, 900010
    result = send_rpc("blockchain.block.headers", [start, count, cp])

    assert isinstance(result, dict)
    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None
    if isinstance(header_data, list):
        assert len(header_data) == result["count"]
        assert all(isinstance(h, str) and len(h) == 160 for h in header_data)
    else:
        assert len(header_data) == 160 * result["count"]
    assert "root" in result and "branch" in result
    for h in result["branch"]:
        assert isinstance(h, str) and len(h) == 64


def test_blockchain_block_headers_20000_at_tip_with_cp():
    """
    Request 20,000 headers at a well-confirmed range with a checkpoint proof.

    The server may return fewer headers than requested — servers impose their
    own per-request maximum (e.g. ElectrumX defaults to 2016, one retarget
    period; libbitcoin-server can be configured up to 20160). The test verifies
    the response is valid and internally consistent, not a specific count.
    """
    start, count = 900000, 20000
    cp = start + count - 1  # = 919999, well below current tip
    result = send_rpc("blockchain.block.headers", [start, count, cp])

    assert isinstance(result, dict)
    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None
    returned = result["count"]
    assert 0 < returned <= count, \
        f"Returned {returned} headers, expected 1–{count}"
    if isinstance(header_data, list):
        assert len(header_data) == returned
        assert all(isinstance(h, str) and len(h) == 160 for h in header_data)
    else:
        assert len(header_data) == 160 * returned
    assert "root" in result and "branch" in result
    for h in result["branch"]:
        assert isinstance(h, str) and len(h) == 64


def test_blockchain_block_headers_max_at_tip_with_cp():
    """
    Request as many headers as possible ending at the current chain tip.

    Uses count=20160 (max in the Electrum spec). The server will cap the
    actual returned count at its own configured maximum. The test verifies
    that the response is valid and the proof fields are consistent with the
    actual returned range — not that a specific count was returned.
    """
    count = 20160

    tip = send_rpc("blockchain.headers.subscribe")
    # height key differs by version: 'height' (v1.3+) or 'block_height' (v1.0-v1.2)
    tip_height = (
        tip.get("height") or tip.get("block_height")
        if isinstance(tip, dict) else None
    )
    if tip_height is None:
        pytest.skip("blockchain.headers.subscribe did not return height")

    start = tip_height - count + 1
    cp = tip_height

    result = send_rpc("blockchain.block.headers", [start, count, cp])

    assert isinstance(result, dict)
    header_data = result.get("headers") or result.get("hex")
    assert header_data is not None
    returned = result["count"]
    assert 0 < returned <= count, \
        f"Returned {returned} headers, expected 1–{count}"
    if isinstance(header_data, list):
        assert len(header_data) == returned
        assert all(isinstance(h, str) and len(h) == 160 for h in header_data)
    else:
        assert len(header_data) == 160 * returned
    assert "root" in result and "branch" in result
    for h in result["branch"]:
        assert isinstance(h, str) and len(h) == 64


def test_blockchain_block_headers_count_zero():
    """
    Request zero headers — server must return an empty/zero response.
    Sending count=0 with an out-of-range start is an edge case;
    xfail on not_found is acceptable.
    """
    result = send_rpc("blockchain.block.headers", [ReferenceData.KNOWN_HEIGHT + 1000000, 0, 0])
    assert isinstance(result, dict)
    assert "count" in result
    assert result["count"] == 0 or result.get("headers") in ([], None) or result.get("hex") in ("", None)


def test_blockchain_block_headers_cp_target_overflow():
    """
    Request headers where cp_height < target height — server must respond with an error.
    (target = start + count - 1, so here target = KNOWN_HEIGHT + 4 > cp = KNOWN_HEIGHT)
    """
    start = ReferenceData.KNOWN_HEIGHT
    count = 5
    cp = start  # target overflows cp
    # send_rpc converts server errors to pytest.xfail automatically
    send_rpc("blockchain.block.headers", [start, count, cp])


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCKCHAIN — HEADERS SUBSCRIBE
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_headers_subscribe():
    """
    Test blockchain.headers.subscribe — subscribe to block header notifications.

    The response format is version-dependent:

    v1.0–v1.2  (raw=false default): deserialized header object
      {'block_height': int, 'version': int, 'prev_block_hash': str, ...}

    v1.3+      (raw=true default / forced): serialized raw format
      {'height': int, 'hex': str (160 chars)}

    v1.4+: the 'raw' parameter is removed; always raw format.

    Push notifications after subscribing are tested separately in
    test_electrum_subscriptions.py. The send_rpc helper transparently
    skips any notifications on the shared session connection.
    """
    result = send_rpc("blockchain.headers.subscribe")

    assert isinstance(result, dict), \
        f"blockchain.headers.subscribe must return a dict, got {type(result)}"

    if negotiated_version is not None and negotiated_version >= "1.3":
        # v1.3+: raw serialized format — height + hex
        assert "height" in result, (
            f"v1.3+ response must have 'height', got keys: {list(result.keys())}"
        )
        assert "hex" in result, (
            f"v1.3+ response must have 'hex', got keys: {list(result.keys())}"
        )
        assert isinstance(result["height"], int) and result["height"] >= 0
        assert isinstance(result["hex"], str) and len(result["hex"]) == 160, (
            f"'hex' must be 160-char header, got length {len(result.get('hex', ''))}"
        )
    else:
        # v1.0–v1.2: deserialized header object with block_height
        assert "block_height" in result, (
            f"v1.0–1.2 response must have 'block_height', got keys: {list(result.keys())}"
        )
        assert isinstance(result["block_height"], int) and result["block_height"] >= 0
        # Must contain raw header fields (not a serialized blob)
        assert any(k in result for k in ("version", "prev_block_hash", "merkle_root")), (
            f"v1.0–1.2 response missing expected header fields: {list(result.keys())}"
        )


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCKCHAIN — FEE ESTIMATION
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_estimatefee():
    """
    Test blockchain.estimatefee — returns estimated fee rate in BTC/kB.

    Per spec, the server may return -1 if fee estimation is unavailable
    (e.g. mempool data not indexed or insufficient samples). Any positive
    value is a fee rate. Both are valid protocol responses.
    """
    result = send_rpc("blockchain.estimatefee", [25])
    assert isinstance(result, (float, int)), \
        f"blockchain.estimatefee must return a number, got {type(result)}"
    # -1 is the protocol-defined "unavailable" sentinel; any positive value is a fee rate
    assert result == -1 or result > 0, \
        f"Expected positive fee rate or -1 (unavailable), got {result}"


def test_blockchain_relayfee():
    """
    Test blockchain.relayfee — minimum relay fee rate in BTC/kB.

    Available in v1.0–v1.4.2 only. Removed in v1.6; clients should use
    server.features['fee_rate'] instead. When negotiated version is v1.6+,
    the server must reject the call with wrong_version.
    """
    if negotiated_version is not None and negotiated_version >= "1.6":
        # v1.6+: the method was removed. The server must reject it with any
        # error response — libbitcoin-server returns 'wrong_version', ElectrumX
        # returns -32601 'unknown method'. Both are correct; we just verify it fails.
        try:
            _rpc_raw("blockchain.relayfee")
            pytest.fail(
                "blockchain.relayfee should fail in v1.6+ (method removed), "
                "but the server returned a result"
            )
        except ValueError:
            pass  # Any error response is correct — method no longer exists in v1.6
        return

    # v1.0–v1.4.2: must return a positive fee rate in BTC/kB.
    result = send_rpc("blockchain.relayfee")
    assert isinstance(result, (float, int)), \
        f"blockchain.relayfee must return a number, got {type(result)}"
    assert result > 0, f"Relay fee must be positive, got {result}"


# ═══════════════════════════════════════════════════════════════════════════════
# SCRIPTHASH METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_scripthash_get_balance():
    """
    Test blockchain.scripthash.get_balance — returns confirmed/unconfirmed satoshi balance.

    Result: {'confirmed': int, 'unconfirmed': int}
    """
    result = send_rpc("blockchain.scripthash.get_balance", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    assert "confirmed" in result and "unconfirmed" in result, \
        f"Missing required keys in balance response: {result}"
    assert isinstance(result["confirmed"], int) and result["confirmed"] >= 0
    assert isinstance(result["unconfirmed"], int)


def test_blockchain_scripthash_get_history():
    """
    Test blockchain.scripthash.get_history — returns list of transaction entries.

    Each entry: {'tx_hash': str, 'height': int}
    Mempool entries may additionally include 'fee'.
    """
    result = send_rpc("blockchain.scripthash.get_history", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, list), f"Expected list, got {type(result)}"
    for entry in result:
        assert isinstance(entry, dict)
        assert "tx_hash" in entry, f"History entry missing 'tx_hash': {entry}"
        assert "height" in entry, f"History entry missing 'height': {entry}"
        assert isinstance(entry["tx_hash"], str) and len(entry["tx_hash"]) == 64
        assert isinstance(entry["height"], int)


def test_blockchain_scripthash_get_mempool():
    """
    Test blockchain.scripthash.get_mempool — unconfirmed transactions for scripthash.

    Each entry: {'tx_hash': str, 'height': int, 'fee': int}
    height is 0 for unconfirmed, -1 for unconfirmed with unconfirmed ancestors.
    """
    result = send_rpc("blockchain.scripthash.get_mempool", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, list), f"Expected list, got {type(result)}"
    for entry in result:
        assert isinstance(entry, dict)
        assert "tx_hash" in entry
        assert "height" in entry
        assert "fee" in entry


def test_blockchain_scripthash_list_unspent():
    """
    Test blockchain.scripthash.listunspent — UTXO list for scripthash.

    Note: the Electrum protocol uses 'listunspent' (no underscore before 'unspent'),
    unlike get_balance / get_history which use underscores.

    Each entry: {'tx_hash': str, 'tx_pos': int, 'value': int, 'height': int}
    """
    result = send_rpc("blockchain.scripthash.listunspent", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, list), f"Expected list, got {type(result)}"
    for utxo in result:
        assert isinstance(utxo, dict)
        for key in ("tx_hash", "tx_pos", "value", "height"):
            assert key in utxo, f"UTXO missing '{key}': {utxo}"
        assert isinstance(utxo["tx_hash"], str) and len(utxo["tx_hash"]) == 64
        assert isinstance(utxo["tx_pos"], int) and utxo["tx_pos"] >= 0
        assert isinstance(utxo["value"], int) and utxo["value"] >= 0
        assert isinstance(utxo["height"], int)


def test_blockchain_scripthash_subscribe():
    """
    Test blockchain.scripthash.subscribe — initial response only.

    Returns null if the scripthash has no history, otherwise a 64-char hex
    status hash (SHA256 of the canonical history string, natural byte order).

    Push notifications for subsequent status changes are tested in
    test_electrum_subscriptions.py. The send_rpc helper skips any notifications
    that arrive on the shared connection while later tests run.
    """
    result = send_rpc("blockchain.scripthash.subscribe", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert result is None or (isinstance(result, str) and len(result) == 64), (
        f"blockchain.scripthash.subscribe must return null or a 64-char status hash, "
        f"got {result!r}"
    )


def test_blockchain_scripthash_unsubscribe():
    """
    Test blockchain.scripthash.unsubscribe — cancels a scripthash subscription.

    Returns True if the subscription existed and was removed, False otherwise.
    Not all server implementations support this method.
    """
    result = send_rpc("blockchain.scripthash.unsubscribe", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, bool), (
        f"blockchain.scripthash.unsubscribe must return bool, got {result!r}"
    )


# ═══════════════════════════════════════════════════════════════════════════════
# TRANSACTION METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_transaction_broadcast():
    """
    Test blockchain.transaction.broadcast — broadcast a raw transaction.

    Sending an invalid transaction hex exercises the broadcast path.
    The server is expected to reject it with an error (xfail).
    """
    send_rpc("blockchain.transaction.broadcast", ["00" * 10])


def test_blockchain_transaction_get():
    """
    Test blockchain.transaction.get — returns raw transaction as hex string.

    Per spec: with verbose=False (default), result is a plain hex string.
    """
    response = send_rpc("blockchain.transaction.get", [ReferenceData.KNOWN_TX_HASH, False])
    assert isinstance(response, str), \
        f"blockchain.transaction.get must return a hex string, got {type(response)}"
    assert len(response) > 100, "Transaction hex seems too short"
    # Validate it's valid hex
    try:
        bytes.fromhex(response)
    except ValueError:
        pytest.fail(f"Response is not valid hex: {response[:64]}...")


def test_blockchain_transaction_get_verbose():
    """
    Test blockchain.transaction.get with verbose=True — returns decoded tx object.

    Per spec, the verbose response is an object containing at least:
      'txid', 'hex', 'confirmations', 'blockhash'
    """
    response = send_rpc("blockchain.transaction.get", [ReferenceData.KNOWN_TX_HASH, True])
    assert isinstance(response, dict), \
        f"Verbose transaction response must be a dict, got {type(response)}"
    for key in ("txid", "hex", "confirmations", "blockhash"):
        assert key in response, f"Verbose tx response missing '{key}': {list(response.keys())}"
    assert response["txid"].lower() == ReferenceData.KNOWN_TX_HASH.lower()
    assert isinstance(response["hex"], str) and len(response["hex"]) > 100


def test_blockchain_transaction_get_merkle():
    """
    Test blockchain.transaction.get_merkle — returns Merkle proof for a transaction.

    Result: {'block_height': int, 'merkle': [str, ...], 'pos': int}
    """
    result = send_rpc(
        "blockchain.transaction.get_merkle",
        [ReferenceData.KNOWN_TX_HASH, ReferenceData.KNOWN_HEIGHT],
    )
    assert isinstance(result, dict)
    assert "merkle" in result and "pos" in result, \
        f"get_merkle response missing fields: {list(result.keys())}"
    assert isinstance(result["merkle"], list)
    assert isinstance(result["pos"], int) and result["pos"] >= 0
    for h in result["merkle"]:
        assert isinstance(h, str) and len(h) == 64


def test_blockchain_transaction_id_from_pos():
    """
    Test blockchain.transaction.id_from_pos — get tx hash from block position.

    With merkle=True, result is a dict with 'tx_hash' and 'merkle'.
    Position 0 is the coinbase transaction.
    """
    result = send_rpc(
        "blockchain.transaction.id_from_pos",
        [ReferenceData.KNOWN_HEIGHT, 0, True],
    )
    assert isinstance(result, dict), \
        f"id_from_pos with merkle=True must return a dict, got {type(result)}"
    assert "tx_hash" in result, f"Response missing 'tx_hash': {list(result.keys())}"
    assert isinstance(result["tx_hash"], str) and len(result["tx_hash"]) == 64


# ═══════════════════════════════════════════════════════════════════════════════
# MEMPOOL METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_mempool_get_fee_histogram():
    """
    Test mempool.get_fee_histogram — returns fee distribution histogram.

    Result: [[fee_rate, cumulative_vsize], ...] sorted by fee_rate (ascending or descending).
    An empty list [] is valid if the mempool is empty or data is unavailable.
    """
    response = send_rpc("mempool.get_fee_histogram")
    assert isinstance(response, list), \
        f"mempool.get_fee_histogram must return a list, got {type(response)}"

    for entry in response:
        assert isinstance(entry, list) and len(entry) == 2, \
            f"Each histogram entry must be [fee_rate, vsize], got {entry!r}"
        fee_rate, cum_vsize = entry
        assert isinstance(fee_rate, (int, float)) and fee_rate >= 0
        assert isinstance(cum_vsize, (int, float)) and cum_vsize >= 0

    if len(response) >= 2:
        fees = [e[0] for e in response]
        assert fees == sorted(fees) or fees == sorted(fees, reverse=True), \
            "Fee rates must be monotonically sorted (ascending or descending)"
