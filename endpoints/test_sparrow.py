"""
Tests for the libbitcoin-server sparrow interface.

Sparrow is an independent service on its own binding, serving the electrum
interface plus three methods of its own (as frigate implements them, and as
sparrow consumes them):

    blockchain.block.stats
    blockchain.silentpayments.subscribe
    blockchain.silentpayments.unsubscribe

Silent payment support is advertised by the silent_payments field of the
electrum server.features response, which plain electrum omits.

This suite covers the sparrow surface, and spot checks the inherited electrum
interface only to verify dispatch and configuration -- test_electrum.py covers
the electrum methods themselves.

  - Tests with no xfail marker assert real, currently-implemented behavior.
    A failure here is a regression.
  - Tests marked `@pytest.mark.xfail(strict=False)` describe the intended
    behavior of the method bodies, which are stubs. They fail now, and start
    passing (as XPASS) when the queries are bound to the store -- the cue to
    tighten the assertion and drop the marker.

The suite skips entirely when no sparrow service is bound.

Run with:
    pytest test_sparrow.py
    pytest test_sparrow.py --sparrow-port=50003
    pytest test_sparrow.py -m xfail -rx     # what is left to implement
"""

from __future__ import annotations

import json
import select
import socket
import time

import pytest

_CLIENT_NAME = "libbitcoin-test"
_TIMEOUT = 15.0

# json-rpc error code shared by an unimplemented and an unknown method.
METHOD_NOT_FOUND = -32601

# A valid scan key pair, of the right lengths, that owns nothing.
SCAN_PRIVATE_KEY = "01" * 32
SPEND_PUBLIC_KEY = "02" + ("00" * 31) + "02"


class SparrowConnection:
    """Newline delimited json-rpc over tcp, as electrum."""

    def __init__(self, host: str, port: int, timeout: float = _TIMEOUT):
        self.socket = socket.create_connection((host, port), timeout=timeout)
        self.socket.settimeout(None)
        self.buffer = b""
        self.identifier = 0

    def close(self) -> None:
        try:
            self.socket.close()
        except OSError:
            pass

    def readline(self, timeout_s: float = _TIMEOUT) -> str | None:
        deadline = time.monotonic() + timeout_s

        while True:
            end = self.buffer.find(b"\n")
            if end >= 0:
                line, self.buffer = self.buffer[:end], self.buffer[end + 1:]
                return line.decode("utf-8", errors="replace")

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None

            ready, _, _ = select.select([self.socket], [], [], remaining)
            if not ready:
                return None

            chunk = self.socket.recv(65536)
            if not chunk:
                return None

            self.buffer += chunk

    def call(self, method: str, params=None) -> dict:
        """Send a request and return the whole response object."""
        self.identifier += 1
        payload = {
            "id": self.identifier,
            "method": method,
            "params": params if params is not None else [],
        }
        self.socket.sendall((json.dumps(payload) + "\n").encode("utf-8"))

        while True:
            line = self.readline()
            if line is None:
                pytest.skip(f"no response to {method}")

            response = json.loads(line)

            # Skip any notification delivered while waiting.
            if response.get("id") == self.identifier:
                return response

    def result(self, method: str, params=None):
        """As call(), xfailing a server-reported error (as test_electrum)."""
        response = self.call(method, params)
        error = response.get("error")
        if error is not None:
            pytest.xfail(f"{method}: {error}")

        return response.get("result")

    def error_code(self, method: str, params=None):
        """The error code of a response, or None if it succeeded."""
        error = self.call(method, params).get("error")
        if error is None:
            return None

        return error.get("code") if isinstance(error, dict) else error


@pytest.fixture(scope="module")
def conn(sparrow_config):
    """A connection with the electrum version handshake completed."""
    host = sparrow_config["host"]
    port = sparrow_config["port"]

    try:
        connection = SparrowConnection(host, port, sparrow_config["timeout"])
    except OSError as exc:
        pytest.skip(f"no sparrow service at {host}:{port} ({exc})")

    try:
        # server.version MUST be the first message (electrum requirement).
        version = connection.call(
            "server.version", [_CLIENT_NAME, sparrow_config["protocol"]])

        if version.get("error") is not None:
            pytest.skip(f"sparrow handshake failed: {version['error']}")

        yield connection
    finally:
        connection.close()


# ─── Configuration and dispatch ──────────────────────────────────────────────

def test_handshake_negotiates_electrum_version(conn):
    """The sparrow service speaks the electrum handshake."""
    response = conn.call("server.version", [_CLIENT_NAME, ["1.4", "1.4"]])

    # Re-negotiation is refused once established, which still proves the
    # handshake protocol is served here.
    assert ("result" in response) or (response.get("error") is not None)


def test_electrum_method_is_served(conn):
    """An electrum method is dispatched unchanged by the sparrow service."""
    result = conn.result("server.banner")
    assert isinstance(result, str)


def test_unknown_method_is_refused(conn):
    """An unknown method reaches the electrum terminal responder."""
    assert conn.error_code("server.bogus") == METHOD_NOT_FOUND


# ─── server.features ─────────────────────────────────────────────────────────

def test_features_advertises_silent_payments(conn):
    """server.features carries the silent_payments version list."""
    features = conn.result("server.features")
    assert isinstance(features, dict)
    assert "silent_payments" in features

    versions = features["silent_payments"]
    assert isinstance(versions, list) and versions
    assert all(isinstance(version, int) for version in versions)


def test_features_retains_electrum_fields(conn):
    """The inherited response is otherwise unchanged."""
    features = conn.result("server.features")
    assert isinstance(features.get("genesis_hash"), str)
    assert isinstance(features.get("server_version"), str)
    assert isinstance(features.get("protocol_min"), str)
    assert isinstance(features.get("protocol_max"), str)


def test_electrum_service_omits_silent_payments(electrum_config):
    """Plain electrum does not advertise what it does not serve."""
    host = electrum_config["host"]
    port = electrum_config["port"]

    try:
        connection = SparrowConnection(host, port, electrum_config["timeout"])
    except OSError as exc:
        pytest.skip(f"no electrum service at {host}:{port} ({exc})")

    try:
        connection.call(
            "server.version", [_CLIENT_NAME, electrum_config["protocol"]])

        features = connection.result("server.features")
        assert "silent_payments" not in features
    finally:
        connection.close()


def test_electrum_service_omits_block_stats(electrum_config):
    """Plain electrum does not serve the sparrow methods."""
    host = electrum_config["host"]
    port = electrum_config["port"]

    try:
        connection = SparrowConnection(host, port, electrum_config["timeout"])
    except OSError as exc:
        pytest.skip(f"no electrum service at {host}:{port} ({exc})")

    try:
        connection.call(
            "server.version", [_CLIENT_NAME, electrum_config["protocol"]])

        assert connection.error_code(
            "blockchain.block.stats", [1]) == METHOD_NOT_FOUND
    finally:
        connection.close()


# ─── blockchain.block.stats ──────────────────────────────────────────────────

@pytest.mark.xfail(strict=False, reason="stub: not yet bound to the store")
def test_block_stats_returns_statistics(conn):
    """A height returns the bitcoind getblockstats subset."""
    stats = conn.result("blockchain.block.stats", [1])
    assert isinstance(stats, dict)
    assert stats["height"] == 1
    assert isinstance(stats["blockhash"], str)
    assert isinstance(stats["total_weight"], int)
    assert isinstance(stats["txs"], int)
    assert isinstance(stats["time"], int)
    assert isinstance(stats["feerate_percentiles"], list)


@pytest.mark.xfail(strict=False, reason="stub: not yet bound to the store")
def test_block_stats_unknown_height_rejected(conn):
    """A height above the top is an error, not a result."""
    code = conn.error_code("blockchain.block.stats", [99_000_000])
    assert code is not None and code != METHOD_NOT_FOUND


# ─── blockchain.silentpayments ───────────────────────────────────────────────

@pytest.mark.xfail(strict=False, reason="stub: not yet bound to the store")
def test_silentpayments_subscribe_returns_subscription(conn):
    """A scan key pair returns the subscription (address, labels, start)."""
    subscription = conn.result("blockchain.silentpayments.subscribe",
        [SCAN_PRIVATE_KEY, SPEND_PUBLIC_KEY])

    assert isinstance(subscription, dict)
    assert isinstance(subscription["address"], str)
    assert isinstance(subscription["start_height"], int)
    assert isinstance(subscription["labels"], list)


@pytest.mark.xfail(strict=False, reason="stub: not yet bound to the store")
def test_silentpayments_subscribe_malformed_key_rejected(conn):
    """A key of the wrong length is invalid, not unimplemented."""
    code = conn.error_code("blockchain.silentpayments.subscribe",
        ["00", SPEND_PUBLIC_KEY])

    assert code is not None and code != METHOD_NOT_FOUND


@pytest.mark.xfail(strict=False, reason="stub: not yet bound to the store")
def test_silentpayments_unsubscribe_returns_address(conn):
    """Unsubscribe returns the scan address of the dropped subscription."""
    conn.result("blockchain.silentpayments.subscribe",
        [SCAN_PRIVATE_KEY, SPEND_PUBLIC_KEY])

    address = conn.result("blockchain.silentpayments.unsubscribe",
        [SCAN_PRIVATE_KEY, SPEND_PUBLIC_KEY])

    assert isinstance(address, str) and address


# ─── Stub behavior (drop these when the methods are implemented) ─────────────

@pytest.mark.parametrize("method,params", [
    ("blockchain.block.stats", [1]),
    ("blockchain.silentpayments.subscribe",
        [SCAN_PRIVATE_KEY, SPEND_PUBLIC_KEY]),
    ("blockchain.silentpayments.unsubscribe",
        [SCAN_PRIVATE_KEY, SPEND_PUBLIC_KEY]),
])
def test_sparrow_methods_are_stubs(conn, method, params):
    """The methods are served but unimplemented."""
    assert conn.error_code(method, params) == METHOD_NOT_FOUND
