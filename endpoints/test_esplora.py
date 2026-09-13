"""
Tests for libbitcoin-server esplora REST API.

Tests the Blockstream explorer API surface including transactions, addresses,
blocks, mempool and fee estimates.

Run with:
    pytest test_esplora.py
    pytest test_esplora.py --esplora-host=192.168.1.100 --esplora-port=3000
"""

import pytest
import requests
from typing import Any, Dict, List, Union

from utils import ReferenceData, TestConfig, validate_hex_hash

JsonLike = Union[Dict[str, Any], List[Any], str, int, float, bool, None]

TIMEOUT = TestConfig.DEFAULT_HTTP_TIMEOUT

# The interface fixes these page sizes, they are not parameters.
CHAIN_PAGE = 25
BLOCK_PAGE = 10
TX_PAGE = 25


def get(base_url: str, endpoint: str) -> requests.Response:
    """GET the endpoint, returning the raw response."""
    return requests.get(f"{base_url}/{endpoint}", timeout=TIMEOUT,
                        headers={"Connection": "close"})


def get_json(base_url: str, endpoint: str) -> JsonLike:
    """GET the endpoint, asserting success and returning parsed json."""
    response = get(base_url, endpoint)
    assert response.status_code == 200, f"{endpoint}: {response.status_code}"
    return response.json()


def get_text(base_url: str, endpoint: str) -> str:
    """GET the endpoint, asserting success and returning the text body."""
    response = get(base_url, endpoint)
    assert response.status_code == 200, f"{endpoint}: {response.status_code}"
    return response.text.strip()


def post(base_url: str, endpoint: str, body: str) -> requests.Response:
    """POST the body to the endpoint, returning the raw response."""
    return requests.post(f"{base_url}/{endpoint}", data=body, timeout=TIMEOUT,
                         headers={"Connection": "close",
                                  "Content-Type": "text/plain"})


# ═══════════════════════════════════════════════════════════════════════════════
# TIP
# ═══════════════════════════════════════════════════════════════════════════════

def test_tip_height(esplora_config):
    """GET /blocks/tip/height returns the decimal top height."""
    height = get_text(esplora_config["base_url"], "blocks/tip/height")
    assert height.isdigit()
    assert int(height) >= ReferenceData.KNOWN_HEIGHT


def test_tip_hash(esplora_config):
    """GET /blocks/tip/hash returns the top block hash."""
    hash_ = get_text(esplora_config["base_url"], "blocks/tip/hash")
    assert validate_hex_hash(hash_)


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCKS
# ═══════════════════════════════════════════════════════════════════════════════

def test_block_height_genesis(esplora_config):
    """GET /block-height/0 returns the genesis hash."""
    hash_ = get_text(esplora_config["base_url"], "block-height/0")
    assert hash_ == ReferenceData.GENESIS_HASH


def test_block_by_hash(esplora_config):
    """GET /block/:hash returns the block object."""
    block = get_json(esplora_config["base_url"],
                     f"block/{ReferenceData.BLOCK1_HASH}")
    assert block["id"] == ReferenceData.BLOCK1_HASH
    assert block["height"] == ReferenceData.BLOCK1_HEIGHT
    assert block["tx_count"] == 1
    assert block["previousblockhash"] == ReferenceData.GENESIS_HASH
    assert block["size"] > 0
    assert block["weight"] > 0
    assert "merkle_root" in block
    assert "mediantime" in block
    assert "difficulty" in block


def test_block_genesis_has_no_previous(esplora_config):
    """The genesis block object omits previousblockhash."""
    block = get_json(esplora_config["base_url"],
                     f"block/{ReferenceData.GENESIS_HASH}")
    assert block["height"] == ReferenceData.GENESIS_HEIGHT
    assert "previousblockhash" not in block


def test_block_unknown_hash(esplora_config):
    """An unknown block hash is not found."""
    response = get(esplora_config["base_url"], f"block/{'0' * 64}")
    assert response.status_code == 404


def test_block_raw_matches_size(esplora_config):
    """GET /block/:hash/raw returns the serialized block."""
    base_url = esplora_config["base_url"]
    block = get_json(base_url, f"block/{ReferenceData.BLOCK1_HASH}")
    response = get(base_url, f"block/{ReferenceData.BLOCK1_HASH}/raw")
    assert response.status_code == 200
    assert len(response.content) == block["size"]


def test_block_header(esplora_config):
    """GET /block/:hash/header returns the 80 byte header as hex."""
    header = get_text(esplora_config["base_url"],
                      f"block/{ReferenceData.BLOCK1_HASH}/header")
    assert len(header) == 160
    assert int(header, 16) >= 0


def test_block_status(esplora_config):
    """GET /block/:hash/status reports chain membership and successor."""
    status = get_json(esplora_config["base_url"],
                      f"block/{ReferenceData.BLOCK1_HASH}/status")
    assert status["in_best_chain"] is True
    assert validate_hex_hash(status["next_best"])


def test_block_txids(esplora_config):
    """GET /block/:hash/txids returns every txid in the block."""
    txids = get_json(esplora_config["base_url"],
                     f"block/{ReferenceData.BLOCK1_HASH}/txids")
    assert txids == [ReferenceData.BLOCK1_COINBASE_TX_HASH]


def test_block_txid_by_index(esplora_config):
    """GET /block/:hash/txid/:index returns that txid."""
    txid = get_text(esplora_config["base_url"],
                    f"block/{ReferenceData.BLOCK1_HASH}/txid/0")
    assert txid == ReferenceData.BLOCK1_COINBASE_TX_HASH


def test_block_txid_above_count(esplora_config):
    """An index beyond the block's tx count is not found."""
    response = get(esplora_config["base_url"],
                   f"block/{ReferenceData.BLOCK1_HASH}/txid/1")
    assert response.status_code == 404


def test_block_txs(esplora_config):
    """GET /block/:hash/txs returns decoded transactions."""
    txs = get_json(esplora_config["base_url"],
                   f"block/{ReferenceData.KNOWN_BLOCK_HASH}/txs")
    assert isinstance(txs, list)
    assert 0 < len(txs) <= TX_PAGE
    assert validate_hex_hash(txs[0]["txid"])


def test_block_txs_unaligned_start(esplora_config):
    """A start index that is not a multiple of the page size is rejected."""
    response = get(esplora_config["base_url"],
                   f"block/{ReferenceData.KNOWN_BLOCK_HASH}/txs/3")
    assert response.status_code == 400


def test_blocks_from_tip(esplora_config):
    """GET /blocks returns the newest blocks, descending."""
    blocks = get_json(esplora_config["base_url"], "blocks")
    assert len(blocks) == BLOCK_PAGE
    heights = [block["height"] for block in blocks]
    assert heights == sorted(heights, reverse=True)


def test_blocks_from_height(esplora_config):
    """GET /blocks/:height starts the page at that height."""
    blocks = get_json(esplora_config["base_url"],
                      f"blocks/{ReferenceData.KNOWN_HEIGHT}")
    assert len(blocks) == BLOCK_PAGE
    assert blocks[0]["height"] == ReferenceData.KNOWN_HEIGHT


# ═══════════════════════════════════════════════════════════════════════════════
# TRANSACTIONS
# ═══════════════════════════════════════════════════════════════════════════════

def test_tx(esplora_config):
    """GET /tx/:txid returns the decoded transaction with resolved prevouts."""
    tx = get_json(esplora_config["base_url"],
                  f"tx/{ReferenceData.FIRST_TX_HASH}")
    assert tx["txid"] == ReferenceData.FIRST_TX_HASH
    assert tx["vin"] and tx["vout"]
    assert tx["size"] > 0
    assert tx["weight"] > 0
    assert tx["status"]["confirmed"] is True
    assert tx["status"]["block_height"] == ReferenceData.FIRST_TX_BLOCK_HEIGHT

    # The first transaction spends a coinbase output, so it carries a prevout.
    assert tx["vin"][0]["is_coinbase"] is False
    assert tx["vin"][0]["prevout"]["value"] > 0
    assert tx["vout"][0]["scriptpubkey_type"] == "p2pk"


def test_tx_coinbase(esplora_config):
    """A coinbase transaction has no prevout and no fee."""
    tx = get_json(esplora_config["base_url"],
                  f"tx/{ReferenceData.BLOCK1_COINBASE_TX_HASH}")
    assert tx["vin"][0]["is_coinbase"] is True
    assert tx["fee"] == 0


def test_tx_unknown(esplora_config):
    """An unknown txid is not found."""
    response = get(esplora_config["base_url"], f"tx/{'0' * 64}")
    assert response.status_code == 404


def test_tx_hex_and_raw_agree(esplora_config):
    """The hex and binary forms of a transaction are the same bytes."""
    base_url = esplora_config["base_url"]
    hexadecimal = get_text(base_url, f"tx/{ReferenceData.FIRST_TX_HASH}/hex")
    response = get(base_url, f"tx/{ReferenceData.FIRST_TX_HASH}/raw")
    assert response.status_code == 200
    assert response.content.hex() == hexadecimal


def test_tx_status(esplora_config):
    """GET /tx/:txid/status returns the confirmation status."""
    status = get_json(esplora_config["base_url"],
                      f"tx/{ReferenceData.FIRST_TX_HASH}/status")
    assert status["confirmed"] is True
    assert status["block_height"] == ReferenceData.FIRST_TX_BLOCK_HEIGHT
    assert validate_hex_hash(status["block_hash"])


def test_tx_outspends(esplora_config):
    """GET /tx/:txid/outspends reports one entry per output."""
    base_url = esplora_config["base_url"]
    tx = get_json(base_url, f"tx/{ReferenceData.FIRST_TX_HASH}")
    spends = get_json(base_url, f"tx/{ReferenceData.FIRST_TX_HASH}/outspends")
    assert len(spends) == len(tx["vout"])
    assert all(isinstance(spend["spent"], bool) for spend in spends)


def test_tx_outspend(esplora_config):
    """GET /tx/:txid/outspend/:vout reports one output's spender."""
    spend = get_json(esplora_config["base_url"],
                     f"tx/{ReferenceData.FIRST_TX_HASH}/outspend/0")
    assert isinstance(spend["spent"], bool)
    if spend["spent"]:
        assert validate_hex_hash(spend["txid"])
        assert spend["vin"] >= 0


def test_tx_merkle_proof(esplora_config):
    """GET /tx/:txid/merkle-proof returns the electrum-form proof."""
    proof = get_json(esplora_config["base_url"],
                     f"tx/{ReferenceData.FIRST_TX_HASH}/merkle-proof")
    assert proof["block_height"] == ReferenceData.FIRST_TX_BLOCK_HEIGHT
    assert proof["pos"] >= 0
    assert all(validate_hex_hash(item) for item in proof["merkle"])


def test_tx_merkleblock_proof(esplora_config):
    """GET /tx/:txid/merkleblock-proof returns a merkleblock, header first."""
    base_url = esplora_config["base_url"]
    header = get_text(base_url,
                      f"block/{ReferenceData.BLOCK1_HASH}/header")
    proof = get_text(
        base_url,
        f"tx/{ReferenceData.BLOCK1_COINBASE_TX_HASH}/merkleblock-proof")
    assert proof.startswith(header)
    assert len(proof) > len(header)


# ═══════════════════════════════════════════════════════════════════════════════
# BROADCAST
# ═══════════════════════════════════════════════════════════════════════════════

def test_broadcast_get_not_allowed(esplora_config):
    """GET on a broadcast target is not allowed."""
    assert get(esplora_config["base_url"], "tx").status_code == 405
    assert get(esplora_config["base_url"], "txs/package").status_code == 405


def test_broadcast_invalid_encoding(esplora_config):
    """A body that is not transaction hex is rejected."""
    response = post(esplora_config["base_url"], "tx", "xxxx")
    assert response.status_code == 400
    assert response.text


def test_broadcast_package_not_implemented(esplora_config):
    """Package broadcast awaits the transaction pool."""
    response = post(esplora_config["base_url"], "txs/package", "[]")
    assert response.status_code == 501


# ═══════════════════════════════════════════════════════════════════════════════
# ADDRESSES
# ═══════════════════════════════════════════════════════════════════════════════

def test_address(esplora_config):
    """GET /address/:address returns the chain and mempool statistics."""
    address = get_json(esplora_config["base_url"],
                       f"address/{ReferenceData.EXAMPLE_ADDRESS}")
    assert address["address"] == ReferenceData.EXAMPLE_ADDRESS

    chain = address["chain_stats"]
    assert chain["funded_txo_count"] > 0
    assert chain["funded_txo_sum"] >= chain["spent_txo_sum"]
    assert chain["funded_txo_count"] >= chain["spent_txo_count"]
    assert chain["tx_count"] > 0

    # There is no transaction pool.
    assert address["mempool_stats"]["tx_count"] == 0


def test_address_undecodable(esplora_config):
    """An address that does not decode is rejected."""
    response = get(esplora_config["base_url"], "address/notanaddress")
    assert response.status_code == 400


def test_address_txs(esplora_config):
    """GET /address/:address/txs returns decoded transactions, newest first."""
    txs = get_json(esplora_config["base_url"],
                   f"address/{ReferenceData.EXAMPLE_ADDRESS}/txs")
    assert 0 < len(txs) <= CHAIN_PAGE
    heights = [tx["status"]["block_height"] for tx in txs]
    assert heights == sorted(heights, reverse=True)


def test_address_txs_chain_paging(esplora_config):
    """The confirmed page continues from the last seen txid."""
    base_url = esplora_config["base_url"]
    first = get_json(base_url,
                     f"address/{ReferenceData.EXAMPLE_ADDRESS}/txs/chain")
    assert 0 < len(first) <= CHAIN_PAGE
    if len(first) < CHAIN_PAGE:
        pytest.skip("address has a single page of confirmed history")

    last_seen = first[-1]["txid"]
    second = get_json(
        base_url,
        f"address/{ReferenceData.EXAMPLE_ADDRESS}/txs/chain/{last_seen}")
    assert len(second) <= CHAIN_PAGE

    firsts = {tx["txid"] for tx in first}
    assert not firsts.intersection({tx["txid"] for tx in second})


def test_address_txs_mempool(esplora_config):
    """There is no transaction pool, so the unconfirmed page is empty."""
    txs = get_json(esplora_config["base_url"],
                   f"address/{ReferenceData.EXAMPLE_ADDRESS}/txs/mempool")
    assert txs == []


def test_address_utxo(esplora_config):
    """GET /address/:address/utxo returns the unspent outputs."""
    utxos = get_json(esplora_config["base_url"],
                     f"address/{ReferenceData.EXAMPLE_ADDRESS}/utxo")
    assert isinstance(utxos, list)
    for utxo in utxos:
        assert validate_hex_hash(utxo["txid"])
        assert utxo["vout"] >= 0
        assert utxo["value"] > 0
        assert utxo["status"]["confirmed"] is True


def test_scripthash_unfunded(esplora_config):
    """An unfunded script hash reports zeroed statistics."""
    scripthash = "0" * 64
    address = get_json(esplora_config["base_url"],
                       f"scripthash/{scripthash}")
    assert address["scripthash"] == scripthash
    assert address["chain_stats"]["funded_txo_count"] == 0
    assert address["chain_stats"]["tx_count"] == 0


def test_scripthash_utxo_unfunded(esplora_config):
    """An unfunded script hash has no unspent outputs."""
    utxos = get_json(esplora_config["base_url"],
                     f"scripthash/{'0' * 64}/utxo")
    assert utxos == []


# ═══════════════════════════════════════════════════════════════════════════════
# MEMPOOL AND FEES
# ═══════════════════════════════════════════════════════════════════════════════

def test_mempool(esplora_config):
    """GET /mempool returns backlog statistics."""
    mempool = get_json(esplora_config["base_url"], "mempool")
    assert mempool["count"] == 0
    assert mempool["vsize"] == 0
    assert mempool["total_fee"] == 0
    assert mempool["fee_histogram"] == []


def test_mempool_txids(esplora_config):
    """There is no transaction pool, so the txid list is empty."""
    assert get_json(esplora_config["base_url"], "mempool/txids") == []


def test_mempool_recent(esplora_config):
    """There is no transaction pool, so the recent list is empty."""
    assert get_json(esplora_config["base_url"], "mempool/recent") == []


def test_fee_estimates(esplora_config):
    """GET /fee-estimates returns an object keyed by confirmation target."""
    estimates = get_json(esplora_config["base_url"], "fee-estimates")
    assert isinstance(estimates, dict)
    for target, rate in estimates.items():
        assert target.isdigit()
        assert rate >= 0


# ═══════════════════════════════════════════════════════════════════════════════
# TARGETS
# ═══════════════════════════════════════════════════════════════════════════════

def test_invalid_target(esplora_config):
    """An unrecognized target is not found."""
    assert get(esplora_config["base_url"], "bogus").status_code == 404


def test_invalid_component(esplora_config):
    """An unrecognized component of a valid target is not found."""
    assert get(esplora_config["base_url"], "mempool/bogus").status_code == 404
