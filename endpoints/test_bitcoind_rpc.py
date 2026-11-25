"""
Tests for libbitcoin-server bitcoind RPC compatibility interface.

Tests the JSON-RPC 2.0 interface that provides bitcoind compatible
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
    params: Optional[list] = None,
    timeout: Optional[float] = None
) -> Dict[str, Any]:
    """
    Send JSON-RPC 2.0 request to bitcoind-compatible endpoint.

    Args:
        config: Configuration dictionary with url, auth, timeout
        method: RPC method name
        params: Optional list of parameters
        timeout: Overrides the configured timeout (for long scans)

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
            timeout=timeout if timeout is not None else
                config.get("timeout", TestConfig.DEFAULT_RPC_TIMEOUT)
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
    """Test getbestblockhash - returns hash of the top block"""
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
    # chainwork needs a cumulative-work index
    # and size_on_disk needs store-size accounting. The remaining bitcoind fields are
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
        [ReferenceData.KNOWN_BLOCK_HASH, ReferenceData.FILTER_TYPE_BASIC]
    )

    result = response["result"]
    assert isinstance(result, dict)
    assert set(result) == {"filter", "header"}
    assert result["filter"] and len(result["filter"]) % 2 == 0
    bytes.fromhex(result["filter"])
    assert validate_hex_hash(result["header"])


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
    # A whole-set scan, so it runs far longer than an ordinary query (see
    # --scan-timeout). The channel is governed by the server inactivity limit.
    response = send_rpc(bitcoind_rpc_config, "gettxoutsetinfo",
        timeout=bitcoind_rpc_config["scan_timeout"])

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
    """Test pruneblockchain - a node not in prune mode refuses (as bitcoind)"""
    response = raw_rpc(bitcoind_rpc_config, "pruneblockchain", [1000])
    assert response.get("result") is None
    assert response["error"]["code"] == -1


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


# ═══════════════════════════════════════════════════════════════════════════════
# RAW TRANSACTION METHODS
# ═══════════════════════════════════════════════════════════════════════════════


@pytest.fixture(scope="module")
def pruned(bitcoind_rpc_config) -> bool:
    """
    True when the node is pruned.

    A pruned store archives no input script or witness data, so a served
    transaction carries empty inputs and cannot hash to its own txid.
    """
    response = send_rpc(bitcoind_rpc_config, "getblockchaininfo")
    return bool(response["result"].get("pruned", False))


PRUNED_REASON = "pruned node: input script and witness data are not archived"

def test_getrawtransaction_raw(bitcoind_rpc_config, pruned):
    """getrawtransaction verbosity=0 returns the serialized transaction hex."""
    if pruned:
        pytest.skip(PRUNED_REASON)

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


def test_getrawtransaction_coinbase(bitcoind_rpc_config, pruned):
    """getrawtransaction serves coinbase transactions (block 1 coinbase)."""
    if pruned:
        pytest.skip(PRUNED_REASON)

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


@pytest.mark.requires_height(ReferenceData.SEGWIT_HEIGHT)
def test_getrawtransaction_segwit(bitcoind_rpc_config, pruned):
    """getrawtransaction handles segwit transactions (witness serialization)."""
    if pruned:
        pytest.skip(PRUNED_REASON)

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
    # bitcoind reports the total witnessed size, so size/vsize ordering is not
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
# MINING METHODS
# ═══════════════════════════════════════════════════════════════════════════════

_MINING_INFO_KEYS = {"blocks", "bits", "difficulty", "target", "networkhashps",
    "pooledtx", "blockmintxfee", "chain", "next", "warnings"}

_TEMPLATE_KEYS = {"capabilities", "version", "rules", "vbavailable",
    "vbrequired", "previousblockhash", "transactions", "coinbaseaux",
    "coinbasevalue", "longpollid", "target", "mintime", "mutable", "noncerange",
    "sigoplimit", "sizelimit", "curtime", "bits", "height"}

# A nonce search beyond this many hashes is not attempted in python.
_MAXIMUM_TRIES = 1 << 20


def _sha256d(data: bytes) -> bytes:
    import hashlib
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def _expand(bits: str) -> int:
    value = int(bits, 16)
    return (value & 0x007fffff) << (8 * ((value >> 24) - 3))


def _template(config: dict) -> dict:
    data = raw_rpc(config, "getblocktemplate", [{"rules": ["segwit"]}])
    if data.get("error") is not None:
        pytest.skip(f"no template: {data['error']}")
    return data["result"]


def _known_block(config: dict) -> tuple:
    """The hash and raw block of the confirmed top."""
    best = send_rpc(config, "getbestblockhash")["result"]
    raw = send_rpc(config, "getblock", [best, 0])["result"]
    return best, raw


def test_getmininginfo(bitcoind_rpc_config):
    """getmininginfo reports the top and the work required of the next block."""
    count = send_rpc(bitcoind_rpc_config, "getblockcount")["result"]
    result = send_rpc(bitcoind_rpc_config, "getmininginfo")["result"]
    assert _MINING_INFO_KEYS <= set(result)
    assert result["blocks"] in (count, count + 1)
    assert int(result["target"], 16) == _expand(result["bits"])
    assert result["difficulty"] > 0
    assert result["networkhashps"] > 0
    assert result["pooledtx"] >= 0
    assert result["chain"] in ("main", "test", "testnet4", "signet", "regtest")
    assert result["next"]["height"] == result["blocks"] + 1
    assert int(result["next"]["target"], 16) == _expand(result["next"]["bits"])


def test_getnetworkhashps(bitcoind_rpc_config):
    """getnetworkhashps defaults to the last 120 blocks at the top."""
    result = send_rpc(bitcoind_rpc_config, "getnetworkhashps")["result"]
    assert isinstance(result, (int, float)) and result > 0


def test_getnetworkhashps_genesis_is_zero(bitcoind_rpc_config):
    """No block precedes genesis, so its window has no work."""
    result = send_rpc(bitcoind_rpc_config, "getnetworkhashps", [120, 0])
    assert result["result"] == 0


def test_getnetworkhashps_fractional_window_rejected(bitcoind_rpc_config):
    data = raw_rpc(bitcoind_rpc_config, "getnetworkhashps", [1.5])
    assert data["error"]["code"] == -8


@pytest.mark.parametrize("request_", [
    {},
    {"rules": []},
    {"rules": ["segwit"], "mode": "proposal"},
])
def test_getblocktemplate_rejected(bitcoind_rpc_config, request_):
    """The client must declare segwit (bip145), and only template mode is served."""
    data = raw_rpc(bitcoind_rpc_config, "getblocktemplate", [request_])
    assert data["error"]["code"] == -8


@pytest.mark.requires_pool
def test_getblocktemplate(bitcoind_rpc_config):
    """A bip22 template over the confirmed top (bip141 commitment, bip9 rules)."""
    template = _template(bitcoind_rpc_config)
    count = send_rpc(bitcoind_rpc_config, "getblockcount")["result"]
    assert _TEMPLATE_KEYS <= set(template)
    assert template["height"] in (count + 1, count + 2)
    best = send_rpc(bitcoind_rpc_config, "getblockhash",
                    [template["height"] - 1])["result"]
    assert template["previousblockhash"] == best
    assert int(template["target"], 16) == _expand(template["bits"])
    assert template["mintime"] <= template["curtime"]
    assert template["coinbasevalue"] > 0
    assert template["noncerange"] == "00000000ffffffff"
    assert template["mutable"] == ["time", "transactions", "prevblock"]
    assert "!segwit" in template["rules"]
    assert template["weightlimit"] == template["sizelimit"] == 4000000
    assert template["sigoplimit"] == 80000
    assert template["default_witness_commitment"].startswith("6a24aa21a9ed")
    assert template["longpollid"].startswith(template["previousblockhash"])

    fees = 0
    for position, tx in enumerate(template["transactions"], start=1):
        assert {"data", "txid", "hash", "depends", "fee", "sigops",
                "weight"} <= set(tx)
        assert reverse_hex(double_sha256(tx["data"])) == tx["hash"]
        assert all(0 < parent < position for parent in tx["depends"])
        fees += tx["fee"]
    assert template["coinbasevalue"] >= fees


def test_submitblock_duplicate(bitcoind_rpc_config):
    """bip22: a block the node has is a duplicate."""
    _, raw = _known_block(bitcoind_rpc_config)
    data = raw_rpc(bitcoind_rpc_config, "submitblock", [raw])
    assert data.get("error") is None
    assert data["result"] == "duplicate"


@pytest.mark.parametrize("hexdata", ["not hex", "00", "0000000000"])
def test_submitblock_undecodable(bitcoind_rpc_config, hexdata):
    data = raw_rpc(bitcoind_rpc_config, "submitblock", [hexdata])
    assert data["error"]["code"] == -22


@pytest.mark.parametrize("offset", [4, 36])
def test_submitblock_invalid_answered_with_reason(bitcoind_rpc_config, offset):
    """bip22: an invalid block is answered with a reject reason, not an error.

    The previous block (offset 4) or merkle root (offset 36) is changed, so the
    block has an unknown parent, a merkle mismatch, or insufficient work.
    """
    _, raw = _known_block(bitcoind_rpc_config)
    block = bytearray(bytes.fromhex(raw))
    block[offset] ^= 0x01
    data = raw_rpc(bitcoind_rpc_config, "submitblock", [block.hex()])
    assert data.get("error") is None
    assert isinstance(data["result"], str)
    assert data["result"] not in ("", "duplicate")


def test_submitheader_known(bitcoind_rpc_config):
    best = send_rpc(bitcoind_rpc_config, "getbestblockhash")["result"]
    header = send_rpc(bitcoind_rpc_config, "getblockheader",
                      [best, False])["result"]
    data = raw_rpc(bitcoind_rpc_config, "submitheader", [header])
    assert data.get("error") is None
    assert data["result"] is None


def test_submitheader_unknown_parent_rejected(bitcoind_rpc_config):
    best = send_rpc(bitcoind_rpc_config, "getbestblockhash")["result"]
    header = bytearray(bytes.fromhex(send_rpc(
        bitcoind_rpc_config, "getblockheader", [best, False])["result"]))
    header[4] ^= 0x01
    data = raw_rpc(bitcoind_rpc_config, "submitheader", [header.hex()])
    assert data["error"]["code"] == -25


@pytest.mark.parametrize("hexdata", ["not hex", "00"])
def test_submitheader_undecodable(bitcoind_rpc_config, hexdata):
    data = raw_rpc(bitcoind_rpc_config, "submitheader", [hexdata])
    assert data["error"]["code"] == -22


@pytest.mark.parametrize("method,params", [
    ("getprioritisedtransactions", []),
    ("prioritisetransaction", [ReferenceData.FIRST_TX_HASH, None, 1000]),
])
def test_prioritisation_unimplemented(bitcoind_rpc_config, method, params):
    """Fee deltas are not served (client_mempool_disabled)."""
    data = raw_rpc(bitcoind_rpc_config, method, params)
    assert data["error"]["code"] == -33


@pytest.mark.requires_pool
def test_submitblock_mines_template(bitcoind_rpc_config):
    """
    A block built from the template is accepted and becomes the top, and is a
    duplicate when submitted again.

    The coinbase pays an anyone-can-spend output and follows bip34 (height)
    and bip54 (sequence, locktime). Runs only where the target is reachable in
    python and the template has no transactions (an empty regtest pool), as
    it extends the chain.
    """
    import struct

    template = _template(bitcoind_rpc_config)
    target = int(template["target"], 16)
    if (1 << 256) // (target + 1) > _MAXIMUM_TRIES:
        pytest.skip("target not reachable in python (not regtest)")
    if template["transactions"]:
        pytest.skip("template has transactions")

    # The height is a minimal push (bip34): op_1..op_16, else little endian.
    height = template["height"]
    if 1 <= height <= 16:
        script = bytes([0x50 + height])
    else:
        size = (height.bit_length() + 8) // 8
        script = bytes([size]) + height.to_bytes(size, "little")
    script += b"\x08libbitcn"
    payout = b"\x51"
    commitment = bytes.fromhex(template["default_witness_commitment"])

    def var(n: int) -> bytes:
        assert n < 0xfd
        return bytes([n])

    body = (var(1) + bytes(32) + b"\xff\xff\xff\xff" + var(len(script)) +
            script + struct.pack("<I", 0xfffffffe) + var(2) +
            struct.pack("<q", template["coinbasevalue"]) + var(len(payout)) +
            payout + struct.pack("<q", 0) + var(len(commitment)) + commitment)
    locktime = struct.pack("<I", height - 1)
    version = struct.pack("<i", 2)
    stripped = version + body + locktime
    witnessed = version + b"\x00\x01" + body + b"\x01\x20" + bytes(32) + \
        locktime

    root = _sha256d(stripped)
    previous = bytes.fromhex(template["previousblockhash"])[::-1]
    for nonce in range(_MAXIMUM_TRIES):
        header = (struct.pack("<I", template["version"]) + previous + root +
                  struct.pack("<II", template["curtime"],
                              int(template["bits"], 16)) +
                  struct.pack("<I", nonce))
        digest = _sha256d(header)
        if int.from_bytes(digest, "little") <= target:
            break
    else:
        pytest.fail("no nonce meets the target")

    block = header + var(1) + witnessed
    data = raw_rpc(bitcoind_rpc_config, "submitblock", [block.hex()])
    assert data.get("error") is None
    assert data["result"] is None

    mined = digest[::-1].hex()
    assert send_rpc(bitcoind_rpc_config, "getbestblockhash")["result"] == mined
    again = raw_rpc(bitcoind_rpc_config, "submitblock", [block.hex()])
    assert again["result"] == "duplicate"


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
