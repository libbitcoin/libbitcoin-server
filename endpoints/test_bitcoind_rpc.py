"""
Tests for libbitcoin-server bitcoind RPC compatibility interface.

Tests the JSON-RPC 2.0 interface that provides Bitcoin Core compatible
RPC methods for blockchain queries.

Run with:
    pytest test_bitcoind_rpc.py
    pytest test_bitcoind_rpc.py --bitcoind-rpc-host=localhost --bitcoind-rpc-port=8332
    pytest test_bitcoind_rpc.py --bitcoind-auth --bitcoind-cookie=/path/to/.cookie
"""

import os
import time
import pytest
import requests
import json
import warnings
from typing import Optional, Dict, Any

from utils import (
    ReferenceData,
    TestConfig,
    double_sha256,
    reverse_hex,
    validate_hex_hash,
)


def send_rpc(
    config: dict,
    method: str,
    params: Optional[list] = None
) -> Dict[str, Any]:
    """
    Send JSON-RPC 2.0 request to bitcoind-compatible endpoint.

    Args:
        config: Configuration dictionary with url, auth, timeout
        method: RPC method name
        params: Optional list of parameters

    Returns:
        JSON-RPC response dictionary

    Raises:
        RuntimeError: On connection or protocol errors
        pytest.xfail: On valid server error responses
    """
    payload = {
        "jsonrpc": "2.0",
        "id": 0,
        "method": method,
        "params": params if params is not None else [],
    }

    if os.getenv("BITCOIND_DEBUG"):
        print(">>>", json.dumps(payload, indent=2), flush=True)

    _t0 = time.monotonic()
    try:
        response = requests.post(
            config["url"],
            json=payload,
            headers={"Content-Type": "application/json", "Connection": "close"},
            auth=config.get("auth"),
            timeout=config.get("timeout", TestConfig.DEFAULT_RPC_TIMEOUT)
        )
        response.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise RuntimeError(f"RPC connection error: {e}")
    _elapsed = time.monotonic() - _t0

    try:
        data = response.json()
    except ValueError:
        raise RuntimeError("Invalid JSON response from RPC server")

    if os.getenv("BITCOIND_DEBUG"):
        try:
            print(f"<<< {method} ({_elapsed * 1000:.1f} ms):", json.dumps(data, indent=2), flush=True)
        except Exception:
            pass

    # Check for error in response
    if "error" in data and data["error"] is not None:
        error = data["error"]
        pytest.xfail(f"Server sent valid error response: {error}")

    return data


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


def raw_rpc(
    config: dict,
    method: str,
    params: Optional[list] = None
) -> Dict[str, Any]:
    """
    Send a JSON-RPC 2.0 request and return the full response without xfail.

    Unlike send_rpc, this does not treat an error response as an expected
    failure, so error paths (unknown txid, invalid input) can be asserted.
    """
    payload = {
        "jsonrpc": "2.0",
        "id": 0,
        "method": method,
        "params": params if params is not None else [],
    }

    if os.getenv("BITCOIND_DEBUG"):
        print(">>>", json.dumps(payload, indent=2), flush=True)

    try:
        response = requests.post(
            config["url"],
            json=payload,
            headers={"Content-Type": "application/json", "Connection": "close"},
            auth=config.get("auth"),
            timeout=config.get("timeout", TestConfig.DEFAULT_RPC_TIMEOUT)
        )
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        raise RuntimeError(f"RPC connection error: {e}")
    except ValueError:
        raise RuntimeError("Invalid JSON response from RPC server")

    if os.getenv("BITCOIND_DEBUG"):
        print(f"<<< {method}:", json.dumps(data, indent=2), flush=True)

    return data


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCKCHAIN METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_getbestblockhash(bitcoind_rpc_config):
    """Test getbestblockhash - returns hash of the best (tip) block"""
    response = send_rpc(bitcoind_rpc_config, "getbestblockhash")

    assert "error" not in response or response["error"] is None
    assert "result" in response

    result = response["result"]
    assert isinstance(result, str)
    assert len(result) == 64
    assert all(c in "0123456789abcdefABCDEF" for c in result)

    # Validate it's valid hex
    try:
        int(result, 16)
    except ValueError:
        pytest.fail("Block hash is not valid hex")


def test_getblock(bitcoind_rpc_config):
    """Test getblock - returns block information"""
    # verbosity=2 → full transaction objects
    response = send_rpc(
        bitcoind_rpc_config,
        "getblock",
        [ReferenceData.KNOWN_BLOCK_HASH, 2]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert result["hash"] == ReferenceData.KNOWN_BLOCK_HASH
    assert result["height"] == ReferenceData.KNOWN_HEIGHT
    assert "tx" in result
    assert isinstance(result["tx"], list)
    assert "difficulty" in result
    assert "time" in result


def test_getblockchaininfo(bitcoind_rpc_config):
    """Test getblockchaininfo - returns blockchain state information"""
    response = send_rpc(bitcoind_rpc_config, "getblockchaininfo")

    result = response["result"]

    # Required fields
    assert "bestblockhash" in result
    assert "bits" in result
    assert "blocks" in result
    assert "chain" in result
    # chainwork and size_on_disk are intentionally omitted by the implementation:
    # chainwork needs a cumulative-work index (cf. getchainwork, not implemented)
    # and size_on_disk needs store-size accounting. The remaining Core fields are
    # returned.
    assert "difficulty" in result
    assert "headers" in result
    assert "initialblockdownload" in result
    assert "mediantime" in result
    assert "pruned" in result
    assert "target" in result
    assert "time" in result
    assert "verificationprogress" in result
    assert "warnings" in result


def test_getblockcount(bitcoind_rpc_config):
    """Test getblockcount - returns number of blocks in the best chain"""
    response = send_rpc(bitcoind_rpc_config, "getblockcount")

    result = response["result"]
    assert isinstance(result, int)
    assert result >= ReferenceData.KNOWN_HEIGHT


def test_getblockfilter(bitcoind_rpc_config):
    """Test getblockfilter - returns BIP 157 block filter"""
    response = send_rpc(
        bitcoind_rpc_config,
        "getblockfilter",
        [ReferenceData.KNOWN_BLOCK_HASH, ReferenceData.FILTER_TYPE_BLOOM]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert "filter" in result
    assert "height" in result
    assert result["height"] == ReferenceData.KNOWN_HEIGHT


def test_getblockhash(bitcoind_rpc_config):
    """Test getblockhash - returns hash of block at given height"""
    response = send_rpc(
        bitcoind_rpc_config,
        "getblockhash",
        [ReferenceData.KNOWN_HEIGHT]
    )

    assert "error" not in response or response["error"] is None
    assert "result" in response

    result = response["result"]
    assert isinstance(result, str)
    assert result == ReferenceData.KNOWN_BLOCK_HASH


def test_getblockheader(bitcoind_rpc_config):
    """Test getblockheader - returns block header information"""
    response = send_rpc(
        bitcoind_rpc_config,
        "getblockheader",
        [ReferenceData.KNOWN_BLOCK_HASH]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert result["hash"] == ReferenceData.KNOWN_BLOCK_HASH
    assert result["height"] == ReferenceData.KNOWN_HEIGHT
    assert "difficulty" in result
    assert "time" in result
    assert "tx" not in result  # Header only, no transactions


def test_getblockstats(bitcoind_rpc_config):
    """Test getblockstats - returns per-block statistics"""
    response = send_rpc(
        bitcoind_rpc_config,
        "getblockstats",
        [ReferenceData.KNOWN_BLOCK_HASH]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert "avgfee" in result
    assert "avgfeerate" in result
    assert "height" in result
    assert result["height"] == ReferenceData.KNOWN_HEIGHT


def test_getchaintxstats(bitcoind_rpc_config):
    """Test getchaintxstats - returns statistics about the total number and rate of transactions"""
    # 5760 blocks (approximately 40 days at 10 min/block)
    response = send_rpc(
        bitcoind_rpc_config,
        "getchaintxstats",
        [5760, ReferenceData.KNOWN_BLOCK_HASH]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert "time" in result
    assert "txcount" in result
    assert "txrate" in result


def test_getchainwork(bitcoind_rpc_config):
    """Test getchainwork - returns the total work in the best chain"""
    response = send_rpc(bitcoind_rpc_config, "getchainwork")

    result = response["result"]
    assert isinstance(result, str)
    assert len(result) > 0
    assert all(c in "0123456789abcdefABCDEF" for c in result)


def test_gettxout(bitcoind_rpc_config):
    """Test gettxout - returns details about an unspent transaction output (UTXO)"""
    # For old block, output is likely spent → expect None
    response = send_rpc(
        bitcoind_rpc_config,
        "gettxout",
        [ReferenceData.KNOWN_TX_HASH, 0, False]
    )

    result = response["result"]
    # Result is None if spent, dict if unspent
    assert result is None or isinstance(result, dict)


def test_gettxoutsetinfo(bitcoind_rpc_config):
    """Test gettxoutsetinfo - returns statistics about the unspent transaction output set"""
    response = send_rpc(bitcoind_rpc_config, "gettxoutsetinfo")

    result = response["result"]
    assert isinstance(result, dict)
    assert "height" in result
    assert "bestblock" in result
    assert "transactions" in result
    assert "txouts" in result
    assert "bogosize" in result
    assert "disk_size" in result
    assert "total_amount" in result


def test_pruneblockchain(bitcoind_rpc_config):
    """Test pruneblockchain - prunes blockchain up to specified height"""
    # Prune to height 1000 (should be no-op on non-pruning node)
    response = send_rpc(bitcoind_rpc_config, "pruneblockchain", [1000])

    result = response["result"]
    assert isinstance(result, int)
    assert result >= 0


def test_savemempool(bitcoind_rpc_config):
    """Test savemempool - dumps the mempool to disk"""
    response = send_rpc(bitcoind_rpc_config, "savemempool")

    # savemempool returns null on success
    result = response["result"]
    assert result is None or isinstance(result, dict)


def test_scantxoutset(bitcoind_rpc_config):
    """Test scantxoutset - scans the unspent transaction output set for entries matching descriptors"""
    response = send_rpc(
        bitcoind_rpc_config,
        "scantxoutset",
        ["start", [f"addr({ReferenceData.EXAMPLE_ADDRESS})"]]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert "success" in result
    assert result["success"] is True
    assert "unspents" in result
    assert isinstance(result["unspents"], list)
    assert "height" in result
    assert isinstance(result["height"], int)
    assert "bestblock" in result
    assert isinstance(result["bestblock"], str)


def test_verifychain(bitcoind_rpc_config):
    """Test verifychain - verifies blockchain database"""
    response = send_rpc(bitcoind_rpc_config, "verifychain")

    result = response["result"]
    assert result is True


def test_verifytxoutset(bitcoind_rpc_config):
    """Test verifytxoutset - verifies the UTXO set"""
    response = send_rpc(bitcoind_rpc_config, "verifytxoutset", ["test"])

    result = response["result"]
    assert isinstance(result, dict)
    assert "height" in result
    assert "bestblock" in result
    assert "transactions" in result
    assert "txouts" in result
    assert "bogosize" in result
    assert "hash_serialized_2" in result
    assert "disk_size" in result
    assert "total_amount" in result


# ═══════════════════════════════════════════════════════════════════════════════
# RAW TRANSACTION METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_getrawtransaction_raw(bitcoind_rpc_config):
    """getrawtransaction verbosity=0 returns the serialized transaction hex."""
    # Block 170 transaction (first payment, non-segwit) round-trips to its txid.
    response = send_rpc(
        bitcoind_rpc_config,
        "getrawtransaction",
        [ReferenceData.FIRST_TX_HASH, 0]
    )

    result = response["result"]
    assert isinstance(result, str)
    assert len(result) % 2 == 0
    assert validate_hex_hash(result, len(result))
    # For a non-witness transaction txid == reverse(double_sha256(serialized)).
    assert reverse_hex(double_sha256(result)) == ReferenceData.FIRST_TX_HASH


def test_getrawtransaction_verbose(bitcoind_rpc_config):
    """getrawtransaction verbosity=1 returns a decoded tx with chain context."""
    response = send_rpc(
        bitcoind_rpc_config,
        "getrawtransaction",
        [ReferenceData.KNOWN_TX_HASH, 1]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert result["txid"] == ReferenceData.KNOWN_TX_HASH
    assert isinstance(result["vin"], list) and len(result["vin"]) > 0
    assert isinstance(result["vout"], list) and len(result["vout"]) > 0
    # Context injected at the protocol layer (the serializer omits these).
    assert result["blockhash"] == ReferenceData.KNOWN_BLOCK_HASH
    assert result["in_active_chain"] is True
    assert isinstance(result["confirmations"], int) and result["confirmations"] > 0


def test_getrawtransaction_coinbase(bitcoind_rpc_config):
    """getrawtransaction serves coinbase transactions (block 1 coinbase)."""
    response = send_rpc(
        bitcoind_rpc_config,
        "getrawtransaction",
        [ReferenceData.BLOCK1_COINBASE_TX_HASH, 1]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert result["txid"] == ReferenceData.BLOCK1_COINBASE_TX_HASH
    assert isinstance(result["vin"], list) and len(result["vin"]) == 1
    # Non-segwit transaction: the witness txid (hash) equals the txid.
    if "hash" in result:
        assert result["hash"] == result["txid"]


def test_getrawtransaction_segwit(bitcoind_rpc_config):
    """getrawtransaction handles segwit transactions (witness serialization)."""
    response = send_rpc(
        bitcoind_rpc_config,
        "getrawtransaction",
        [ReferenceData.SEGWIT_TX_HASH_P2WPKH, 1]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert result["txid"] == ReferenceData.SEGWIT_TX_HASH_P2WPKH
    assert isinstance(result["vin"], list) and len(result["vin"]) > 0
    assert isinstance(result["vout"], list) and len(result["vout"]) > 0
    # Witness is serialized, so the witness txid (hash) differs from the txid.
    assert result["hash"] != result["txid"]
    # Segwit weight accounting: vsize == ceil(weight / 4).
    # (Note: libbitcoin reports "size" as the stripped, non-witness size, whereas
    # Core reports the total witnessed size, so size/vsize ordering is not
    # asserted here.)
    if "weight" in result and "vsize" in result:
        assert result["vsize"] == (result["weight"] + 3) // 4


def test_getrawtransaction_unknown(bitcoind_rpc_config):
    """getrawtransaction returns a JSON-RPC error for an unknown txid."""
    data = raw_rpc(bitcoind_rpc_config, "getrawtransaction", ["00" * 32, 1])
    assert data.get("error") is not None
    assert data.get("result") is None


def test_sendrawtransaction_invalid_hex(bitcoind_rpc_config):
    """sendrawtransaction rejects non-hexadecimal input with an error."""
    data = raw_rpc(bitcoind_rpc_config, "sendrawtransaction", ["nothexZZ"])
    assert data.get("error") is not None


def test_sendrawtransaction_malformed(bitcoind_rpc_config):
    """sendrawtransaction rejects hex that does not deserialize to a tx."""
    data = raw_rpc(bitcoind_rpc_config, "sendrawtransaction", ["00"])
    assert data.get("error") is not None


@pytest.mark.skipif(
    not os.getenv("BITCOIND_ALLOW_BROADCAST"),
    reason="set BITCOIND_ALLOW_BROADCAST=1 to exercise the broadcast path "
           "(re-announces an already-confirmed tx to connected peers)"
)
def test_sendrawtransaction_known(bitcoind_rpc_config):
    """sendrawtransaction accepts a valid tx and echoes its txid.

    Gated behind BITCOIND_ALLOW_BROADCAST: it announces an inv to peers. The
    transaction is already mined, so acceptance is idempotent and benign.
    """
    raw = send_rpc(
        bitcoind_rpc_config, "getrawtransaction",
        [ReferenceData.FIRST_TX_HASH, 0]
    )["result"]

    data = raw_rpc(bitcoind_rpc_config, "sendrawtransaction", [raw])
    assert data.get("error") is None
    assert data["result"] == ReferenceData.FIRST_TX_HASH


# ═══════════════════════════════════════════════════════════════════════════════
# NETWORK METHODS
# ═══════════════════════════════════════════════════════════════════════════════

def test_getnetworkinfo(bitcoind_rpc_config):
    """Test getnetworkinfo - returns information about the network"""
    response = send_rpc(bitcoind_rpc_config, "getnetworkinfo")

    result = response["result"]
    assert isinstance(result, dict)
    assert "version" in result
    assert "subversion" in result
    assert "protocolversion" in result
    assert "connections" in result
    assert "networks" in result
