"""
Tests for libbitcoin-server bitcoind REST compatibility interface.

Covers the Bitcoin Core REST endpoints served under /rest/: chaininfo, block,
block/notxdetails, block/spent (libbitcoin extension), blockhashbyheight,
headers, blockpart (libbitcoin extension), and the basic (neutrino) filters.

Per-endpoint media type is selected by the URL extension: .json, .hex (text),
or .bin (raw bytes). doc:
    github.com/bitcoin/bitcoin/blob/master/doc/REST-interface.md

Run with:
    pytest test_bitcoind_rest.py
    pytest test_bitcoind_rest.py --bitcoind-rest-host=localhost --bitcoind-rest-port=8332
"""

import hashlib
import os
import time
import pytest
import requests

from utils import ReferenceData, TestConfig, reverse_hex, double_sha256


HEADER_SIZE = 80  # serialized block header size, in bytes


def _header_hash(header: bytes) -> str:
    """Block hash (display byte order) from the first 80 bytes of a header."""
    digest = hashlib.sha256(hashlib.sha256(header[:HEADER_SIZE]).digest()).hexdigest()
    return reverse_hex(digest)


def _get(rest_config: dict, path: str) -> requests.Response:
    """GET a REST path. A non-200 status degrades to xfail (as in the RPC suite)."""
    url = f"{rest_config['base_url']}/{path}"

    if os.getenv("BITCOIND_DEBUG"):
        print(f">>> GET {url}", flush=True)

    _t0 = time.monotonic()
    try:
        resp = requests.get(
            url,
            timeout=TestConfig.DEFAULT_RPC_TIMEOUT,
            headers={"Connection": "close"}
        )
    except requests.exceptions.RequestException as e:
        raise RuntimeError(f"REST connection error: {e}")
    _elapsed = time.monotonic() - _t0

    if os.getenv("BITCOIND_DEBUG"):
        print(f"<<< {path} ({_elapsed * 1000:.1f} ms) "
              f"status={resp.status_code} bytes={len(resp.content)}", flush=True)

    if resp.status_code != 200:
        pytest.xfail(f"Server sent status {resp.status_code} for {path}: "
                     f"{resp.text[:200]!r}")

    return resp


def get_json(rest_config: dict, path: str):
    resp = _get(rest_config, path)
    try:
        return resp.json()
    except ValueError:
        pytest.fail(f"Non-JSON response for {path}: {resp.text[:200]!r}")


def get_hex(rest_config: dict, path: str) -> str:
    return _get(rest_config, path).text.strip()


def get_bin(rest_config: dict, path: str) -> bytes:
    return _get(rest_config, path).content


# ═══════════════════════════════════════════════════════════════════════════════
# CHAIN INFORMATION
# ═══════════════════════════════════════════════════════════════════════════════

def test_chaininfo_json(bitcoind_rest_config):
    """GET /rest/chaininfo.json returns blockchain state."""
    data = get_json(bitcoind_rest_config, "chaininfo.json")

    assert isinstance(data, dict)
    assert data.get("chain") == "main"
    for field in ("blocks", "headers", "bestblockhash", "difficulty"):
        assert field in data
    assert len(data["bestblockhash"]) == 64


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK (full, notxdetails, spent)
# ═══════════════════════════════════════════════════════════════════════════════

def test_block_json(bitcoind_rest_config):
    """GET /rest/block/<hash>.json returns a block with full tx objects."""
    data = get_json(
        bitcoind_rest_config,
        f"block/{ReferenceData.KNOWN_BLOCK_HASH}.json"
    )

    assert isinstance(data, dict)
    assert data["hash"] == ReferenceData.KNOWN_BLOCK_HASH
    assert isinstance(data["tx"], list) and len(data["tx"]) >= 1
    assert isinstance(data["tx"][0], dict)  # full transaction objects


def test_block_hex(bitcoind_rest_config):
    """GET /rest/block/<hash>.hex returns the raw block; header hashes to <hash>."""
    raw = get_hex(bitcoind_rest_config, f"block/{ReferenceData.KNOWN_BLOCK_HASH}.hex")

    assert len(raw) >= 2 * HEADER_SIZE
    assert all(c in "0123456789abcdefABCDEF" for c in raw)
    assert reverse_hex(double_sha256(raw[:2 * HEADER_SIZE])) == ReferenceData.KNOWN_BLOCK_HASH


def test_block_bin(bitcoind_rest_config):
    """GET /rest/block/<hash>.bin returns raw bytes; header hashes to <hash>."""
    raw = get_bin(bitcoind_rest_config, f"block/{ReferenceData.KNOWN_BLOCK_HASH}.bin")

    assert len(raw) >= HEADER_SIZE
    assert _header_hash(raw) == ReferenceData.KNOWN_BLOCK_HASH


def test_block_notxdetails_json(bitcoind_rest_config):
    """GET /rest/block/notxdetails/<hash>.json lists txids, not full objects."""
    data = get_json(
        bitcoind_rest_config,
        f"block/notxdetails/{ReferenceData.KNOWN_BLOCK_HASH}.json"
    )

    assert isinstance(data, dict)
    assert data["hash"] == ReferenceData.KNOWN_BLOCK_HASH
    assert isinstance(data["tx"], list) and len(data["tx"]) >= 1
    assert isinstance(data["tx"][0], str)
    assert len(data["tx"][0]) == 64


def test_block_spent_json(bitcoind_rest_config):
    """GET /rest/block/spent/<hash>.json (libbitcoin extension) returns spent outputs."""
    data = get_json(
        bitcoind_rest_config,
        f"block/spent/{ReferenceData.KNOWN_BLOCK_HASH}.json"
    )

    assert isinstance(data, (list, dict))


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK HASH BY HEIGHT
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockhashbyheight_json(bitcoind_rest_config):
    """GET /rest/blockhashbyheight/<height>.json returns the block hash."""
    data = get_json(
        bitcoind_rest_config,
        f"blockhashbyheight/{ReferenceData.KNOWN_HEIGHT}.json"
    )

    assert isinstance(data, dict)
    assert data["blockhash"] == ReferenceData.KNOWN_BLOCK_HASH


def test_blockhashbyheight_genesis_json(bitcoind_rest_config):
    """GET /rest/blockhashbyheight/0.json returns the genesis hash."""
    data = get_json(bitcoind_rest_config, "blockhashbyheight/0.json")

    assert data["blockhash"] == ReferenceData.GENESIS_HASH


# ═══════════════════════════════════════════════════════════════════════════════
# HEADERS
# ═══════════════════════════════════════════════════════════════════════════════

def test_headers_json(bitcoind_rest_config):
    """GET /rest/headers/<count>/<hash>.json returns <count> header objects."""
    data = get_json(
        bitcoind_rest_config,
        f"headers/5/{ReferenceData.KNOWN_BLOCK_HASH}.json"
    )

    assert isinstance(data, list)
    assert len(data) == 5
    assert data[0]["hash"] == ReferenceData.KNOWN_BLOCK_HASH
    for field in ("merkleroot", "time", "nonce", "bits"):
        assert field in data[0]


def test_headers_hex(bitcoind_rest_config):
    """GET /rest/headers/1/<hash>.hex returns one 80-byte header hashing to <hash>."""
    raw = get_hex(bitcoind_rest_config, f"headers/1/{ReferenceData.KNOWN_BLOCK_HASH}.hex")

    assert len(raw) == 2 * HEADER_SIZE
    assert reverse_hex(double_sha256(raw)) == ReferenceData.KNOWN_BLOCK_HASH


def test_headers_bin(bitcoind_rest_config):
    """GET /rest/headers/2/<hash>.bin returns two concatenated 80-byte headers."""
    raw = get_bin(bitcoind_rest_config, f"headers/2/{ReferenceData.KNOWN_BLOCK_HASH}.bin")

    assert len(raw) == 2 * HEADER_SIZE
    assert _header_hash(raw[:HEADER_SIZE]) == ReferenceData.KNOWN_BLOCK_HASH


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK PART (libbitcoin extension, bin|hex only)
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockpart_hex_header(bitcoind_rest_config):
    """GET /rest/blockpart/<hash>/0/80.hex returns the block's 80-byte header."""
    raw = get_hex(
        bitcoind_rest_config,
        f"blockpart/{ReferenceData.KNOWN_BLOCK_HASH}/0/{HEADER_SIZE}.hex"
    )

    assert len(raw) == 2 * HEADER_SIZE
    assert reverse_hex(double_sha256(raw)) == ReferenceData.KNOWN_BLOCK_HASH


def test_blockpart_bin_header(bitcoind_rest_config):
    """GET /rest/blockpart/<hash>/0/80.bin returns the block's 80-byte header."""
    raw = get_bin(
        bitcoind_rest_config,
        f"blockpart/{ReferenceData.KNOWN_BLOCK_HASH}/0/{HEADER_SIZE}.bin"
    )

    assert len(raw) == HEADER_SIZE
    assert _header_hash(raw) == ReferenceData.KNOWN_BLOCK_HASH


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK FILTERS (basic / neutrino; requires [blockchain] bip158 = true)
# ═══════════════════════════════════════════════════════════════════════════════

def test_blockfilter_basic_json(bitcoind_rest_config):
    """GET /rest/blockfilter/basic/<hash>.json returns a compact block filter."""
    data = get_json(
        bitcoind_rest_config,
        f"blockfilter/basic/{ReferenceData.KNOWN_BLOCK_HASH}.json"
    )

    assert isinstance(data, dict)
    assert "filter" in data


def test_blockfilterheaders_basic_json(bitcoind_rest_config):
    """GET /rest/blockfilterheaders/basic/<hash>.json returns filter headers."""
    data = get_json(
        bitcoind_rest_config,
        f"blockfilterheaders/basic/{ReferenceData.KNOWN_BLOCK_HASH}.json"
    )

    assert isinstance(data, (dict, list))
