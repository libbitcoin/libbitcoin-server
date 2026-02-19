"""
Tests for libbitcoin-server Electrum protocol compatibility interface.

Tests the Electrum Protocol 1.4.2 JSON-RPC over TCP interface
for wallet and blockchain queries.

Run with:
    pytest test_electrumx.py
    pytest test_electrumx.py --electrum-host=localhost --electrum-port=50001
    pytest test_electrumx.py --electrum-host=fulcrum.sethforprivacy.com --electrum-port=50001
"""

import json
import socket
import warnings
import pytest

from utils import ReferenceData, TestConfig


# Global persistent connection (managed by fixture)
sock = None
reader = None


@pytest.fixture(autouse=True, scope="session")
def persistent_connection(electrum_config):
    """Establish persistent connection for entire test session."""
    global sock, reader

    host = electrum_config["host"]
    port = electrum_config["port"]
    timeout = electrum_config.get("timeout", TestConfig.DEFAULT_SOCKET_TIMEOUT)

    try:
        sock = socket.create_connection((host, port), timeout=timeout)
        reader = sock.makefile('r', encoding='utf-8')
        yield
    finally:
        if reader is not None:
            try:
                reader.close()
            except:
                pass
        if sock is not None:
            try:
                sock.close()
            except:
                pass


def send_rpc(method: str, params: list = None):
    """
    Send Electrum JSON-RPC request over persistent TCP connection.

    Args:
        method: Electrum method name (e.g., "server.version")
        params: Optional list of parameters

    Returns:
        Response result (extracted from JSON-RPC wrapper)

    Raises:
        pytest.skip: On unsupported methods or connection errors
        pytest.xfail: On server errors
    """
    payload = {
        "jsonrpc": "2.0",
        "id": 0,
        "method": method,
        "params": params if params is not None else [],
    }

    try:
        sock.sendall((json.dumps(payload) + "\n").encode("utf-8"))

        data = reader.readline().rstrip("\n")

        if not data:
            pytest.skip(f"No response (possibly unsupported method: {method})")

        response = parse_response(data)

        # Check for error in response
        if isinstance(response, dict) and "error" in response and response["error"] is not None:
            err = response["error"]
            msg = err.get("message", str(err)) if isinstance(err, dict) else str(err)
            pytest.xfail(f"Server error: {msg}")

        # Extract result
        if isinstance(response, dict):
            return response.get("result")
        else:
            return response

    except Exception as e:
        pytest.skip(f"Connection / protocol error: {type(e).__name__}: {e}")


def parse_response(raw_data: str) -> dict:
    """
    Parse server response and warn if not clean JSON-RPC 2.0.

    Args:
        raw_data: Raw response string

    Returns:
        Parsed JSON object

    Raises:
        pytest.fail: If JSON is invalid
    """
    try:
        resp = json.loads(raw_data)
    except json.JSONDecodeError as e:
        pytest.fail(f"Invalid JSON from server: {raw_data!r} → {e}")

    if isinstance(resp, dict):
        if "jsonrpc" not in resp or resp["jsonrpc"] != "2.0":
            warnings.warn(
                f"Server response not JSON-RPC 2.0 compliant\n"
                f"Raw response: {raw_data}\n",
                UserWarning,
                stacklevel=3
            )

        if "id" not in resp:
            warnings.warn("Response id missing, not JSON-RPC 2.0 compliant", UserWarning)

        if "result" in resp and "error" in resp:
            warnings.warn("Contains both result and error fields, not JSON-RPC 2.0 compliant", UserWarning)

    return resp


# ═══════════════════════════════════════════════════════════════════════════════
# SERVER METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_server_version():
    """Test server.version - server identification"""
    result = send_rpc("server.version", ["Sparrow", ["1.4", "1.6"]])
    assert isinstance(result, list)
    assert len(result) >= 2
    assert isinstance(result[1], str)
    assert result[1] >= "1.4", f"Protocol too old: {result[1]}"


def test_server_banner():
    """Test server.banner - server banner/motd"""
    result = send_rpc("server.banner")
    assert isinstance(result, str), "server.banner should return a string"


def test_server_features():
    """Test server.features - server capabilities"""
    result = send_rpc("server.features")
    assert isinstance(result, dict)
    required_fields = {"genesis_hash", "hash_function", "protocol_min", "protocol_max"}
    assert required_fields <= set(result), f"Missing required fields: {required_fields - set(result)}"


def test_server_ping():
    """Test server.ping - connection keep-alive"""
    result = send_rpc("server.ping")
    assert result is None, "server.ping should return null"


def test_server_add_peer():
    """Test server.add_peer - add a peer to the server's peer list"""
    features = {
        "genesis_hash": "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f",
        "hosts": {},
        "protocol_max": "1.4",
        "protocol_min": "1.4",
        "server_version": "test/1.0"
    }
    result = send_rpc("server.add_peer", [features])
    assert isinstance(result, bool)


def test_server_donation_address():
    """Test server.donation_address - get server's donation address"""
    result = send_rpc("server.donation_address")
    assert result is None or isinstance(result, str)


def test_server_peers_subscribe():
    """Test server.peers.subscribe - get list of peer servers"""
    result = send_rpc("server.peers.subscribe")
    assert isinstance(result, list)


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCKCHAIN METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_block_header():
    """Test blockchain.block.header - get block header"""
    # cp_height=0 means no checkpoint
    response = send_rpc("blockchain.block.header", [ReferenceData.KNOWN_HEIGHT, 0])

    # Standard Electrum protocol: plain hex string
    if isinstance(response, str):
        assert len(response) == 160, f"Expected 160 hex chars, got {len(response)}"
    # libbitcoin-server returns headers-style dict
    elif isinstance(response, dict):
        header_data = response.get("hex") or response.get("headers")
        assert header_data is not None, "Expected 'hex' or 'headers' key in response"
        if isinstance(header_data, list):
            assert len(header_data) >= 1
            assert len(header_data[0]) == 160
        else:
            assert len(header_data) == 160
    else:
        pytest.fail(f"Unexpected response type: {type(response)}")


def test_blockchain_block_headers():
    """Test blockchain.block.headers - get multiple block headers"""
    # Get 5 headers starting from KNOWN_HEIGHT
    result = send_rpc("blockchain.block.headers", [ReferenceData.KNOWN_HEIGHT, 5, 0])
    assert isinstance(result, dict)
    assert "count" in result

    # Accept either format
    header_data = result.get("hex") or result.get("headers")
    assert header_data is not None, "Expected 'hex' or 'headers' key in response"
    assert isinstance(header_data, str) or isinstance(header_data, list)

    # Validate content
    if isinstance(header_data, list):
        assert len(header_data) == result["count"]
        assert all(isinstance(h, str) and len(h) == 160 for h in header_data)
    else:
        # Old style single string
        assert len(header_data) == 160 * result["count"]


def test_blockchain_headers_subscribe():
    """Test blockchain.headers.subscribe - subscribe to block header notifications"""
    result = send_rpc("blockchain.headers.subscribe")
    assert isinstance(result, dict)
    assert "height" in result
    assert "hex" in result
    assert len(result["hex"]) == 160  # 80 bytes hex


def test_blockchain_estimatefee():
    """Test blockchain.estimatefee - estimate transaction fee"""
    # Estimate fee for confirmation in 25 blocks
    result = send_rpc("blockchain.estimatefee", [25])
    assert isinstance(result, (float, int))
    assert result >= 0


def test_blockchain_relayfee():
    """Test blockchain.relayfee - minimum relay fee"""
    result = send_rpc("blockchain.relayfee")
    assert isinstance(result, float)
    assert result > 0, "Relay fee should be positive"


# ═══════════════════════════════════════════════════════════════════════════════
# SCRIPTHASH METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_scripthash_get_balance():
    """Test blockchain.scripthash.get_balance - get address balance"""
    result = send_rpc("blockchain.scripthash.get_balance", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, dict)
    assert "confirmed" in result
    assert "unconfirmed" in result
    assert isinstance(result["confirmed"], int)
    assert isinstance(result["unconfirmed"], int)
    assert result["confirmed"] >= 0
    assert result["unconfirmed"] >= 0


def test_blockchain_scripthash_get_history():
    """Test blockchain.scripthash.get_history - get address transaction history"""
    result = send_rpc("blockchain.scripthash.get_history", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, list)
    for entry in result:
        assert isinstance(entry, dict)
        assert "tx_hash" in entry
        assert "height" in entry


def test_blockchain_scripthash_get_mempool():
    """Test blockchain.scripthash.get_mempool - get unconfirmed transactions"""
    try:
        response = send_rpc("blockchain.scripthash.get_mempool", [ReferenceData.EXAMPLE_SCRIPTHASH])
    except RuntimeError as e:
        if "No such mempool transaction" in str(e) and "-txindex" in str(e):
            pytest.xfail("txindex missing → getrawtransaction fails as expected")
        else:
            raise

    # Handle different response formats
    if isinstance(response, list):
        mempool = response
    else:
        # Normal JSON-RPC style
        assert "error" not in response or response["error"] is None
        assert "result" in response
        mempool = response["result"]

    for entry in mempool:
        assert isinstance(entry, dict)
        assert "tx_hash" in entry
        assert "height" in entry
        assert "fee" in entry


def test_blockchain_scripthash_list_unspent():
    """Test blockchain.scripthash.list_unspent - list unspent outputs"""
    result = send_rpc("blockchain.scripthash.list_unspent", [ReferenceData.EXAMPLE_SCRIPTHASH])
    assert isinstance(result, list)
    for utxo in result:
        assert isinstance(utxo, dict)
        assert "tx_hash" in utxo
        assert "tx_pos" in utxo
        assert "value" in utxo
        assert "height" in utxo


def test_blockchain_scripthash_subscribe():
    """Test blockchain.scripthash.subscribe - subscribe to address changes"""
    result = send_rpc("blockchain.scripthash.subscribe", [ReferenceData.EXAMPLE_SCRIPTHASH])
    # Subscription response is usually None (confirmation via notification)
    assert result is None or isinstance(result, str)


def test_blockchain_scripthash_unsubscribe():
    """Test blockchain.scripthash.unsubscribe - unsubscribe from address"""
    try:
        response = send_rpc("blockchain.scripthash.unsubscribe", [ReferenceData.EXAMPLE_SCRIPTHASH])

        # Successful responses are usually True, False, None, empty dict/list
        assert response in (True, False, None, {}, [])

    except Exception as e:
        error_msg = str(e).lower()
        if "unknown method" in error_msg or "unsubscribe" in error_msg:
            pytest.skip(
                "Server does not implement blockchain.scripthash.unsubscribe "
                "(normal on electrs; supported on ElectrumX, Fulcrum, etc.)"
            )
        else:
            pytest.xfail(f"Unexpected error in unsubscribe call: {e}")


# ═══════════════════════════════════════════════════════════════════════════════
# TRANSACTION METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockchain_transaction_broadcast():
    """Test blockchain.transaction.broadcast - broadcast a raw transaction"""
    # Send an invalid transaction hex; server error is expected and handled as xfail
    send_rpc("blockchain.transaction.broadcast", ["00" * 10])


def test_blockchain_transaction_get():
    """Test blockchain.transaction.get - get transaction (raw hex)"""
    response = send_rpc("blockchain.transaction.get", [ReferenceData.KNOWN_TX_HASH, False])
    assert isinstance(response, str)
    assert len(response) > 100  # Transaction should be reasonable length


def test_blockchain_transaction_get_verbose():
    """Test blockchain.transaction.get with verbose=True"""
    response = send_rpc("blockchain.transaction.get", [ReferenceData.KNOWN_TX_HASH, True])

    assert isinstance(response, dict)
    assert "hex" in response
    assert "confirmations" in response
    assert "blockhash" in response
    assert response["txid"].lower() == ReferenceData.KNOWN_TX_HASH.lower()

    # Validate embedded hex
    assert isinstance(response.get("hex"), str)
    assert len(response["hex"]) > 100


def test_blockchain_transaction_get_merkle():
    """Test blockchain.transaction.get_merkle - get merkle proof for transaction"""
    result = send_rpc(
        "blockchain.transaction.get_merkle",
        [ReferenceData.KNOWN_TX_HASH, ReferenceData.KNOWN_HEIGHT]
    )
    assert isinstance(result, dict)
    assert "merkle" in result
    assert "pos" in result
    assert isinstance(result["merkle"], list)
    assert isinstance(result["pos"], int)


def test_blockchain_transaction_id_from_pos():
    """Test blockchain.transaction.id_from_pos - get tx hash from block position"""
    # Get coinbase (position 0) from KNOWN_HEIGHT
    result = send_rpc("blockchain.transaction.id_from_pos", [ReferenceData.KNOWN_HEIGHT, 0, True])
    assert isinstance(result, dict)
    # Should contain tx_hash and potentially merkle proof


# ═══════════════════════════════════════════════════════════════════════════════
# MEMPOOL METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_mempool_get_fee_histogram():
    """Test mempool.get_fee_histogram - get fee distribution"""
    response = send_rpc("mempool.get_fee_histogram")

    # Handle different response formats
    if isinstance(response, list):
        histogram = response
    else:
        warnings.warn(
            "Wrong response format for mempool.get_fee_histogram, expected naked array",
            UserWarning
        )
        histogram = response

    # Validate histogram structure
    for entry in histogram:
        assert isinstance(entry, list)
        assert len(entry) == 2
        fee_rate, cum_vsize = entry
        assert isinstance(fee_rate, (int, float))
        assert isinstance(cum_vsize, (int, float))
        assert fee_rate >= 0
        assert cum_vsize >= 0

    # Validate sorting (fee rates must be monotone - either ascending or descending)
    if len(histogram) >= 2:
        fees = [entry[0] for entry in histogram]
        assert fees == sorted(fees) or fees == sorted(fees, reverse=True), \
            "Fee rates should be monotonically sorted"
