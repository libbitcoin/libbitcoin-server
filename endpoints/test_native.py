"""
Tests for libbitcoin-server native REST API.

Tests the /v1/ endpoints for blockchain exploration including blocks,
transactions, inputs, outputs, and addresses.

Run with:
    pytest test_native.py
    pytest test_native.py --native-host=192.168.1.100 --native-port=8181
"""

import json
import os
import time
import pytest
import requests
from typing import Any, Dict, List, Union

from utils import ReferenceData, TestConfig, validate_hex_hash

JsonLike = Union[Dict[str, Any], List[Any], str, int, float, bool, None]


def get_json(
    base_url: str,
    endpoint: str,
    test_name: str = "",
    timeout: float = TestConfig.DEFAULT_HTTP_TIMEOUT,
    allow_404: bool = False,
    allow_error_status: tuple = (400, 404, 503)
) -> JsonLike:
    """GET → JSON with latency logging and auto-unwrap of [dict] → dict"""
    url = f"{base_url}/{endpoint}?format=json"

    if os.getenv("NATIVE_DEBUG"):
        print(f">>> GET {url}", flush=True)

    _t0 = time.monotonic()
    try:
        resp = requests.get(url, timeout=timeout, headers={"Connection": "close"})
        _elapsed = time.monotonic() - _t0

        if os.getenv("NATIVE_DEBUG"):
            try:
                print(f"<<< {endpoint} ({_elapsed * 1000:.1f} ms):", json.dumps(resp.json(), indent=2), flush=True)
            except Exception:
                pass

        if resp.status_code in allow_error_status:
            if allow_404 and resp.status_code == 404:
                return None
            resp.raise_for_status()

        resp.raise_for_status()
        try:
            data = resp.json()
        except requests.exceptions.JSONDecodeError:
            pytest.skip(f"Non-JSON response from {url} (status {resp.status_code})")

        # Auto-unwrap single-element list containing dict
        if isinstance(data, list) and len(data) == 1 and isinstance(data[0], dict):
            data = data[0]

        return data

    except requests.exceptions.HTTPError as e:
        code = getattr(e.response, 'status_code', '?')
        raise
    except Exception as e:
        raise


# ═══════════════════════════════════════════════════════════════════════════════
# GENERAL / STATUS ENDPOINTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_configuration(native_config):
    """Test /v1/configuration endpoint."""
    data = get_json(native_config["base_url"], "configuration")
    assert isinstance(data, (dict, list, str))
    assert len(data) > 0 if isinstance(data, (dict, list)) else bool(data)


@pytest.mark.requires_height(ReferenceData.KNOWN_HEIGHT)
def test_top(native_config):
    """Test /v1/top endpoint - chain top information."""
    data = get_json(native_config["base_url"], "top")
    assert data[0]["chain"] == "confirmed"
    assert data[0]["height"] >= ReferenceData.KNOWN_HEIGHT


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK ENDPOINTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_block_by_hash(native_config):
    """Test /v1/block/hash/{blockhash}"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}")
    assert isinstance(data, dict)
    assert "header" in data
    header = data["header"]
    assert header["hash"] == ReferenceData.KNOWN_BLOCK_HASH
    assert header["height"] == ReferenceData.KNOWN_HEIGHT


def test_block_by_height(native_config):
    """Test /v1/block/confirmed/{height}"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}")
    assert isinstance(data, dict)
    assert "header" in data
    header = data["header"]
    assert header["height"] == ReferenceData.KNOWN_HEIGHT
    assert header["hash"] == ReferenceData.KNOWN_BLOCK_HASH


def test_block_header_by_hash(native_config):
    """Test /v1/block/hash/{blockhash}/header"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/header")
    assert isinstance(data, dict)
    assert data["hash"] == ReferenceData.KNOWN_BLOCK_HASH


def test_block_header_by_height(native_config):
    """Test /v1/block/confirmed/{height}/header"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}/header")
    assert isinstance(data, dict)
    assert data["height"] == ReferenceData.KNOWN_HEIGHT


def test_header_context_by_hash(native_config):
    """Test /v1/block/hash/{blockhash}/header/context"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/header/context")
    if data is not None:
        assert isinstance(data, dict)
        assert data["height"] == ReferenceData.KNOWN_HEIGHT


def test_header_context_by_height(native_config):
    """Test /v1/block/confirmed/{height}/header/context"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}/header/context")
    if data is not None:
        assert isinstance(data, dict)
        assert data["height"] == ReferenceData.KNOWN_HEIGHT


def test_block_details_by_hash(native_config):
    """Test /v1/block/hash/{blockhash}/details"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/details")
    assert isinstance(data, dict)
    assert data["height"] == ReferenceData.KNOWN_HEIGHT


def test_block_details_by_height(native_config):
    """Test /v1/block/confirmed/{height}/details"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}/details")
    assert isinstance(data, dict)
    assert data["hash"] == ReferenceData.KNOWN_BLOCK_HASH


def test_block_txs_by_hash(native_config):
    """Test /v1/block/hash/{blockhash}/txs - all transactions in block"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/txs")
    assert isinstance(data, list), "txs endpoint should return list of tx hashes"
    assert len(data) >= 1
    assert isinstance(data[0], str), "Expected list of tx hashes (strings)"
    assert data[0] == ReferenceData.KNOWN_TX_HASH, f"First tx should be {ReferenceData.KNOWN_TX_HASH}"


def test_block_txs_by_height(native_config):
    """Test /v1/block/confirmed/{height}/txs"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}/txs")
    assert isinstance(data, list)
    assert len(data) >= 1
    assert isinstance(data[0], str)


@pytest.mark.requires_height(ReferenceData.FIRST_TX_BLOCK_HEIGHT)
def test_block_txs_details_by_height(native_config):
    """Test /v1/block/confirmed/{height}/txs/details - coinbase and an outside spend"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.FIRST_TX_BLOCK_HEIGHT}/txs/details")
    assert data == ReferenceData.FIRST_TX_BLOCK_DETAILS


@pytest.mark.requires_height(ReferenceData.FIRST_TX_BLOCK_HEIGHT)
def test_block_txs_details_by_hash(native_config):
    """Test /v1/block/hash/{blockhash}/txs/details"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.FIRST_TX_BLOCK_HASH}/txs/details")
    assert data == ReferenceData.FIRST_TX_BLOCK_DETAILS


def test_block_txs_details_text_not_acceptable(native_config):
    """Test /v1/block/confirmed/{height}/txs/details is json only"""
    url = f"{native_config['base_url']}/block/confirmed/0/txs/details?format=text"
    assert requests.get(url, timeout=TestConfig.DEFAULT_HTTP_TIMEOUT).status_code == 406


def test_block_tx_by_position_hash(native_config):
    """Test /v1/block/hash/{blockhash}/tx/{position}"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/tx/0")
    assert isinstance(data, dict)
    assert "hash" in data


def test_block_tx_by_position_height(native_config):
    """Test /v1/block/confirmed/{height}/tx/{position}"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}/tx/0")
    assert isinstance(data, dict)
    assert "hash" in data


def test_block_tx_input_script_height(native_config):
    """Test /v1/block/confirmed/{height}/tx/{position}/input/{index}/script"""
    data = get_json(native_config["base_url"], f"block/confirmed/{ReferenceData.KNOWN_HEIGHT}/tx/0/input/0/script")
    assert isinstance(data, (str, dict))


def test_block_tx_output_script_hash(native_config):
    """Test /v1/block/hash/{blockhash}/tx/{position}/output/{index}/script"""
    data = get_json(native_config["base_url"], f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/tx/0/output/0/script")
    assert isinstance(data, (str, dict))


# ═══════════════════════════════════════════════════════════════════════════════
# BLOCK FILTER ENDPOINTS (BIP 158)
# ═══════════════════════════════════════════════════════════════════════════════

@pytest.mark.parametrize("filter_type", [ReferenceData.FILTER_TYPE_NEUTRINO])
def test_block_filter_by_hash(native_config, filter_type):
    """Test /v1/block/hash/{blockhash}/filter/{type}"""
    try:
        data = get_json(
            native_config["base_url"],
            f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/filter/{filter_type}",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (dict, str))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip(f"Filter type '{filter_type}' not implemented")
        raise


@pytest.mark.parametrize("filter_type", [ReferenceData.FILTER_TYPE_NEUTRINO])
def test_block_filter_hash_by_hash(native_config, filter_type):
    """Test /v1/block/hash/{blockhash}/filter/{type}/hash"""
    try:
        data = get_json(
            native_config["base_url"],
            f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/filter/{filter_type}/hash",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, str)
            assert len(data) == 64  # SHA-256 hash
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip(f"Filter hash for type '{filter_type}' not implemented")
        raise


@pytest.mark.parametrize("filter_type", [ReferenceData.FILTER_TYPE_NEUTRINO])
def test_block_filter_header_by_hash(native_config, filter_type):
    """Test /v1/block/hash/{blockhash}/filter/{type}/header"""
    try:
        data = get_json(
            native_config["base_url"],
            f"block/hash/{ReferenceData.KNOWN_BLOCK_HASH}/filter/{filter_type}/header",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (dict, str))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip(f"Filter header for type '{filter_type}' not implemented")
        raise


# ═══════════════════════════════════════════════════════════════════════════════
# TRANSACTION ENDPOINTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_tx_by_hash(native_config):
    """Test /v1/tx/{txhash}"""
    data = get_json(native_config["base_url"], f"tx/{ReferenceData.KNOWN_TX_HASH}")
    assert isinstance(data, dict)
    assert data["hash"] == ReferenceData.KNOWN_TX_HASH


def test_tx_header(native_config):
    """Test /v1/tx/{txhash}/header - block header of confirming block"""
    try:
        data = get_json(
            native_config["base_url"],
            f"tx/{ReferenceData.KNOWN_TX_HASH}/header",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, dict)
            assert "hash" in data or "height" in data
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("tx/header endpoint not implemented")
        raise


def test_tx_details(native_config):
    """Test /v1/tx/{txhash}/details - extended transaction information"""
    try:
        data = get_json(
            native_config["base_url"],
            f"tx/{ReferenceData.KNOWN_TX_HASH}/details",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, dict)
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("tx/details endpoint not implemented")
        raise



# ═══════════════════════════════════════════════════════════════════════════════
# INPUT ENDPOINTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_inputs_by_tx(native_config):
    """Test /v1/input/{txhash} - all inputs in transaction"""
    data = get_json(native_config["base_url"], f"input/{ReferenceData.KNOWN_TX_HASH}")

    if isinstance(data, dict):
        # Single input case
        assert "point" in data or "previous_output" in data
        assert "script" in data or "sequence" in data
        assert data.get("sequence", 0) == 4294967295  # Coinbase typical
    elif isinstance(data, list):
        # List of inputs
        assert len(data) >= 1
        assert isinstance(data[0], dict)
        assert "point" in data[0] or "script" in data[0]
    else:
        pytest.fail(f"Unexpected type for inputs: {type(data)}")


def test_single_input(native_config):
    """Test /v1/input/{txhash}/{index}"""
    data = get_json(native_config["base_url"], f"input/{ReferenceData.KNOWN_TX_HASH}/0")
    assert isinstance(data, dict)


def test_input_script(native_config):
    """Test /v1/input/{txhash}/{index}/script"""
    try:
        data = get_json(
            native_config["base_url"],
            f"input/{ReferenceData.KNOWN_TX_HASH}/0/script",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (str, dict))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("input/script endpoint not implemented")
        raise


@pytest.mark.requires_height(ReferenceData.SEGWIT_HEIGHT)
def test_input_witness(native_config):
    """Test /v1/input/{txhash}/{index}/witness - uses first SegWit block (481824)"""
    try:
        data = get_json(
            native_config["base_url"],
            f"input/{ReferenceData.SEGWIT_TX_HASH_P2WPKH}/0/witness",
            allow_error_status=(400, 404, 501)
        )
        assert data is not None, "Expected witness data for P2WPKH SegWit transaction"
        assert isinstance(data, (str, dict, list))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("input/witness endpoint not implemented")
        raise


# ═══════════════════════════════════════════════════════════════════════════════
# OUTPUT ENDPOINTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_outputs_by_tx(native_config):
    """Test /v1/output/{txhash} - all outputs in transaction"""
    data = get_json(native_config["base_url"], f"output/{ReferenceData.KNOWN_TX_HASH}")

    if isinstance(data, dict):
        # Single output case
        assert "value" in data
        assert "script" in data
        assert data["value"] == 5000000000  # Coinbase reward at height 9000
    elif isinstance(data, list):
        # List of outputs
        assert len(data) >= 1
        assert isinstance(data[0], dict)
        assert "value" in data[0]
        assert data[0]["value"] > 0
    else:
        pytest.fail(f"Unexpected type for outputs: {type(data)}")


def test_single_output(native_config):
    """Test /v1/output/{txhash}/{index}"""
    data = get_json(native_config["base_url"], f"output/{ReferenceData.KNOWN_TX_HASH}/0")
    assert isinstance(data, dict)
    assert "value" in data
    assert data["value"] > 0


def test_output_script(native_config):
    """Test /v1/output/{txhash}/{index}/script"""
    try:
        data = get_json(
            native_config["base_url"],
            f"output/{ReferenceData.KNOWN_TX_HASH}/0/script",
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (str, dict))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("output/script endpoint not implemented")
        raise


@pytest.mark.requires_current
def test_output_spender(native_config):
    """Test /v1/output/{txhash}/{index}/spender - uses first Bitcoin tx (block 170, output 0 spent by Hal Finney)"""
    try:
        data = get_json(
            native_config["base_url"],
            f"output/{ReferenceData.FIRST_TX_HASH}/0/spender",
            allow_error_status=(400, 404, 501)
        )
        assert isinstance(data, dict)
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("output/spender endpoint not implemented")
        raise


@pytest.mark.requires_current
def test_output_spenders(native_config):
    """Test /v1/output/{txhash}/{index}/spenders - uses first Bitcoin tx (block 170, output 0 spent by Hal Finney)"""
    try:
        data = get_json(
            native_config["base_url"],
            f"output/{ReferenceData.FIRST_TX_HASH}/0/spenders",
            allow_error_status=(400, 404, 501)
        )
        assert isinstance(data, (list, dict))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("output/spenders endpoint not implemented")
        raise


# ═══════════════════════════════════════════════════════════════════════════════
# ADDRESS ENDPOINTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_address_all(native_config):
    """Test /v1/address/{scripthash} - all transactions and UTXOs"""
    try:
        data = get_json(
            native_config["base_url"],
            f"address/{ReferenceData.KNOWN_OUTPUT_SCRIPT_HASH}",
            allow_404=True,
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (dict, list))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("501 Not Implemented → feature pending")
        raise


def test_address_confirmed(native_config):
    """Test /v1/address/{scripthash}/confirmed"""
    try:
        data = get_json(
            native_config["base_url"],
            f"address/{ReferenceData.KNOWN_OUTPUT_SCRIPT_HASH}/confirmed",
            allow_404=True,
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (dict, list))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("address/confirmed not implemented")
        raise


def test_address_unconfirmed(native_config):
    """Test /v1/address/{scripthash}/unconfirmed"""
    try:
        data = get_json(
            native_config["base_url"],
            f"address/{ReferenceData.KNOWN_OUTPUT_SCRIPT_HASH}/unconfirmed",
            allow_404=True,
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            assert isinstance(data, (dict, list))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("address/unconfirmed not implemented")
        raise


def test_address_balance(native_config):
    """Test /v1/address/{scripthash}/balance"""
    try:
        data = get_json(
            native_config["base_url"],
            f"address/{ReferenceData.KNOWN_OUTPUT_SCRIPT_HASH}/balance",
            allow_404=True,
            allow_error_status=(400, 404, 501)
        )
        if data is not None:
            if isinstance(data, dict):
                assert "confirmed" in data or "balance" in data
            else:
                assert isinstance(data, int)
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 501:
            pytest.skip("address/balance not implemented")
        raise


# ═══════════════════════════════════════════════════════════════════════════════
# ERROR HANDLING TESTS
# ═══════════════════════════════════════════════════════════════════════════════

def test_non_existing_block_hash(native_config):
    """Test 404 response for non-existing block hash"""
    fake_hash = "0000000000000000000000000000000000000000000000000000000000009999"
    with pytest.raises(requests.exceptions.HTTPError):
        get_json(native_config["base_url"], f"block/hash/{fake_hash}")


def test_invalid_height(native_config):
    """Test 404 response for invalid/future block height"""
    with pytest.raises(requests.exceptions.HTTPError):
        get_json(native_config["base_url"], "block/confirmed/10000000")


def test_unsupported_format(native_config):
    """Test error response for unsupported format parameter"""
    url = f"{native_config['base_url']}/top?format=xml"
    resp = requests.get(url, timeout=5)
    assert resp.status_code in (400, 406, 415), "Should reject unsupported format"


# ═══════════════════════════════════════════════════════════════════════════════
# TEMPLATE
# ═══════════════════════════════════════════════════════════════════════════════

def confirmed_top(native_config) -> int:
    """The confirmed top height."""
    return get_json(native_config["base_url"], "top")[0]["height"]


def template_status(native_config, height: int) -> int:
    """The status of the template at height."""
    url = f"{native_config['base_url']}/template/{height}?format=json"
    return requests.get(url, timeout=TestConfig.DEFAULT_HTTP_TIMEOUT).status_code


def template_key_sets(native_config, height: int) -> set:
    """The distinct key sets of the elements of the template at height."""
    url = f"{native_config['base_url']}/template/{height}/txs/details?format=json"
    response = requests.get(url, timeout=TestConfig.DEFAULT_HTTP_TIMEOUT)
    response.raise_for_status()
    return {frozenset(element) for element in response.json()}


def test_template_at_top_not_found(native_config):
    """Test /v1/template/{height} has no template at the confirmed top"""
    assert template_status(native_config, confirmed_top(native_config)) == 404


def test_template_beyond_next_not_found(native_config):
    """Test /v1/template/{height} has no template beyond the next block"""
    assert template_status(native_config, confirmed_top(native_config) + 1000) == 404


def test_template_text_not_acceptable(native_config):
    """Test /v1/template/{height} is json only"""
    url = f"{native_config['base_url']}/template/{confirmed_top(native_config) + 1}?format=text"
    assert requests.get(url, timeout=TestConfig.DEFAULT_HTTP_TIMEOUT).status_code == 406


@pytest.mark.requires_pool
def test_template_next_elements(native_config):
    """Test /v1/template/{height} of the next block has tx details elements"""
    assert template_key_sets(native_config, confirmed_top(native_config) + 1) <= {frozenset(ReferenceData.TX_DETAILS_KEYS)}


def template_summary_keys(native_config, height: int) -> set:
    """The keys of the summary of the template at height."""
    url = f"{native_config['base_url']}/template/{height}?format=json"
    response = requests.get(url, timeout=TestConfig.DEFAULT_HTTP_TIMEOUT)
    response.raise_for_status()
    return set(response.json())


@pytest.mark.requires_pool
def test_template_next_summary(native_config):
    """Test /v1/template/{height} of the next block is its summary"""
    assert template_summary_keys(native_config, confirmed_top(native_config) + 1) == {"height", "count", "virtual", "weight", "fees", "clusters"}
