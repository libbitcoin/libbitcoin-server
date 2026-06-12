"""
Electrum subscription / push-notification tests.

These tests subscribe to server events and wait for real blockchain push
notifications. They require a live node with active network traffic
(new blocks arriving or transactions being broadcast) and much longer
timeouts than the regular request/response tests.

Each test opens its own fresh TCP connection (function-scoped) so that
subscriptions are fully isolated and don't interfere with each other or
with the shared session connection in test_electrum.py.

Run with:

    pytest test_electrum_subscriptions.py
    pytest test_electrum_subscriptions.py --subscription-timeout=120

Optional environment variables:
    ELECTRUM_TRIGGER_HEADERS    Shell command to produce a new block header event
    ELECTRUM_TRIGGER_SCRIPTHASH Shell command to produce activity for EXAMPLE_SCRIPTHASH
    ELECTRUM_DEBUG=1            Pretty-print all JSON-RPC messages
"""

import json
import os
import select
import socket
import subprocess
import time
import pytest
from typing import Any

from utils import ReferenceData, TestConfig

_CLIENT_NAME = "libbitcoin-server-test/1.0"


# ─── Connection class ─────────────────────────────────────────────────────────

class ElectrumConnection:
    """
    Manages a single Electrum TCP connection with a manual line buffer.

    Uses select() + recv() instead of socket.makefile() so that timeouts
    on notification waits never corrupt the reader state.
    """

    def __init__(self, host: str, port: int, connect_timeout: float):
        self.sock = socket.create_connection((host, port), timeout=connect_timeout)
        self.sock.settimeout(None)  # switch to blocking after connect
        self._buf: bytes = b""

    def close(self) -> None:
        try:
            self.sock.close()
        except Exception:
            pass

    def readline(self, timeout_s: float | None = None) -> str | None:
        """Read one newline-delimited line, returning None on timeout or close."""
        deadline = time.monotonic() + timeout_s if timeout_s is not None else None

        while True:
            nl = self._buf.find(b"\n")
            if nl >= 0:
                line, self._buf = self._buf[:nl], self._buf[nl + 1:]
                return line.decode("utf-8", errors="replace")

            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                ready, _, _ = select.select([self.sock], [], [], remaining)
                if not ready:
                    return None

            try:
                chunk = self.sock.recv(65536)
            except OSError:
                return None

            if not chunk:
                return None
            self._buf += chunk

    def send_rpc(self, method: str, params: list | None = None,
                 timeout_s: float = 10.0) -> Any:
        """Send a JSON-RPC request and return the parsed result, or None on error."""
        payload = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": method,
            "params": params if params is not None else [],
        }

        if os.getenv("ELECTRUM_DEBUG"):
            print(">>>", json.dumps(payload, indent=2), flush=True)

        self.sock.sendall((json.dumps(payload) + "\n").encode("utf-8"))

        data = self.readline(timeout_s)
        if data is None:
            return None

        if os.getenv("ELECTRUM_DEBUG"):
            print(f"<<< {method}:", data, flush=True)

        try:
            resp = json.loads(data)
        except json.JSONDecodeError:
            return None

        if isinstance(resp, dict) and resp.get("error") is not None:
            return None
        if isinstance(resp, dict) and "result" in resp:
            return resp["result"]
        return resp

    def read_notification(self, timeout_s: float) -> dict | None:
        """
        Read the next server-pushed notification within timeout_s seconds.

        Skips regular RPC responses (those have a 'result' key) and returns
        only objects that look like notifications (have 'method', no 'result').
        Returns None on timeout.
        """
        deadline = time.monotonic() + timeout_s
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None

            data = self.readline(remaining)
            if data is None:
                return None

            if os.getenv("ELECTRUM_DEBUG"):
                print("<<< [notification]:", data, flush=True)

            try:
                obj = json.loads(data)
            except json.JSONDecodeError:
                continue

            if isinstance(obj, dict) and "method" in obj and "result" not in obj:
                return obj


# ─── Fixture ──────────────────────────────────────────────────────────────────

@pytest.fixture
def conn(electrum_config: dict) -> ElectrumConnection:
    """
    Fresh Electrum connection per test, with version handshake.
    Closes the connection after the test regardless of outcome.
    """
    host = electrum_config["host"]
    port = electrum_config["port"]
    timeout = electrum_config.get("timeout", TestConfig.DEFAULT_SOCKET_TIMEOUT)
    protocol = electrum_config.get("protocol", ["1.4", "1.6"])

    try:
        c = ElectrumConnection(host, port, connect_timeout=timeout)
    except OSError as exc:
        pytest.skip(f"Cannot connect to {host}:{port}: {exc}")

    result = c.send_rpc("server.version", [_CLIENT_NAME, protocol], timeout_s=timeout)
    if result is None:
        c.close()
        pytest.skip("server.version handshake failed; server unreachable or not Electrum")

    yield c
    c.close()


# ─── Tests ────────────────────────────────────────────────────────────────────

def test_headers_notification(conn: ElectrumConnection, electrum_config: dict):
    """
    Subscribe to blockchain.headers and wait for a real push notification.

    The test subscribes, optionally triggers an external command (via
    ELECTRUM_TRIGGER_HEADERS env var) to produce a new block, then waits
    up to --subscription-timeout seconds for the server to push a header.

    Notification format (Electrum protocol):
        {'method': 'blockchain.headers.subscribe', 'params': [{'height': int, 'hex': str}]}
    """
    sub_timeout = electrum_config.get("subscription_timeout", 60.0)

    initial = conn.send_rpc("blockchain.headers.subscribe", timeout_s=10.0)
    if initial is None:
        pytest.skip("blockchain.headers.subscribe returned no response")

    assert isinstance(initial, dict) and "height" in initial, (
        f"Expected {{'height': int, 'hex': str}} from headers.subscribe, got {initial!r}"
    )

    cmd = os.getenv("ELECTRUM_TRIGGER_HEADERS")
    if cmd:
        subprocess.run(cmd, shell=True, check=False)
    else:
        print(
            f"\n  Waiting up to {sub_timeout:.0f}s for a new block notification "
            "(set ELECTRUM_TRIGGER_HEADERS to trigger one immediately).",
            flush=True,
        )

    notif = conn.read_notification(timeout_s=sub_timeout)
    if notif is None:
        pytest.skip(
            f"No header notification received within {sub_timeout:.0f}s. "
            "Increase --subscription-timeout or use ELECTRUM_TRIGGER_HEADERS."
        )

    assert notif.get("method") == "blockchain.headers.subscribe", (
        f"Unexpected notification method: {notif.get('method')!r}"
    )
    params = notif.get("params", [])
    assert isinstance(params, list) and params, \
        f"Notification 'params' must be a non-empty list, got {params!r}"
    payload = params[0]
    assert isinstance(payload, dict), \
        f"First element of notification params must be a dict, got {payload!r}"
    assert "height" in payload, f"Notification payload missing 'height': {payload}"
    assert "hex" in payload, f"Notification payload missing 'hex': {payload}"
    assert isinstance(payload["height"], int) and payload["height"] > initial["height"], (
        f"Notification height {payload['height']} must be greater than "
        f"subscribe height {initial['height']}"
    )
    assert isinstance(payload["hex"], str) and len(payload["hex"]) == 160, (
        f"Notification 'hex' must be 160-char header, got length {len(payload.get('hex', ''))}"
    )


def test_scripthash_notification(conn: ElectrumConnection, electrum_config: dict):
    """
    Subscribe to a scripthash and wait for a status push notification.

    Subscribes to EXAMPLE_SCRIPTHASH, optionally triggers an external command
    (via ELECTRUM_TRIGGER_SCRIPTHASH) to produce on-chain activity, then waits
    for the server to push a status update.

    Notification format:
        {'method': 'blockchain.scripthash.subscribe', 'params': [scripthash, status]}
    where status is null (reset) or a 64-char hex SHA256 hash.

    Note: the status hash must be in natural byte order (not reversed).
    See docs/bug-electrum-scripthash-status-byteorder.md.
    """
    sub_timeout = electrum_config.get("subscription_timeout", 60.0)

    initial = conn.send_rpc(
        "blockchain.scripthash.subscribe",
        [ReferenceData.EXAMPLE_SCRIPTHASH],
        timeout_s=10.0,
    )
    # null = no history; 64-char hex = current status hash
    assert initial is None or (isinstance(initial, str) and len(initial) == 64), (
        f"blockchain.scripthash.subscribe must return null or a 64-char status hash, "
        f"got {initial!r}"
    )

    cmd = os.getenv("ELECTRUM_TRIGGER_SCRIPTHASH")
    if cmd:
        subprocess.run(cmd, shell=True, check=False)
    else:
        print(
            f"\n  Waiting up to {sub_timeout:.0f}s for a scripthash notification "
            "(set ELECTRUM_TRIGGER_SCRIPTHASH to trigger one immediately).",
            flush=True,
        )

    notif = conn.read_notification(timeout_s=sub_timeout)
    if notif is None:
        pytest.skip(
            f"No scripthash notification received within {sub_timeout:.0f}s. "
            "Increase --subscription-timeout or use ELECTRUM_TRIGGER_SCRIPTHASH."
        )

    assert notif.get("method") == "blockchain.scripthash.subscribe", (
        f"Unexpected notification method: {notif.get('method')!r}"
    )
    params = notif.get("params", [])
    assert isinstance(params, list) and len(params) == 2, (
        f"Scripthash notification must have exactly 2 params [scripthash, status], "
        f"got {params!r}"
    )
    scripthash, status = params
    assert scripthash == ReferenceData.EXAMPLE_SCRIPTHASH, (
        f"Notification scripthash mismatch: expected {ReferenceData.EXAMPLE_SCRIPTHASH!r}, "
        f"got {scripthash!r}"
    )
    assert status is None or (isinstance(status, str) and len(status) == 64), (
        f"Notification status must be null or a 64-char hex hash, got {status!r}"
    )
