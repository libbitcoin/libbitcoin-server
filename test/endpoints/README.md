# libbitcoin-server Endpoint Tests

Comprehensive test suite for libbitcoin-server's client-server interfaces.

## Overview

This directory contains Python-based integration tests for all libbitcoin-server network endpoints:

| Interface | Protocol | Test File | Status |
|-----------|----------|-----------|--------|
| **Native REST** | HTTP/S + JSON | `test_native.py` | 🔧 In Progress |
| **bitcoind RPC** | HTTP/S + JSON-RPC 2.0 | `test_bitcoind_rpc.py` | 🔧 In Progress |
| **Electrum** | TCP + JSON-RPC 2.0 | `test_electrum.py` | 🔧 In Progress |
| **bitcoind REST** | HTTP/S + JSON/Binary | `test_bitcoind_rest.py` | 🚧 Planned |
| **Stratum v1** | TCP + JSON-RPC 1.0 | `test_stratum_v1.py` | 🚧 Planned |
| **Stratum v2** | TCP + Binary | `test_stratum_v2.py` | 🚧 Planned |

## Prerequisites

### 1. Install Dependencies

```bash
pip install pytest requests
```

### 2. Running libbitcoin-server

Ensure libbitcoin-server is running with the desired endpoints enabled. See [configuration.md](../../docs/configuration.md) for endpoint setup.

**Minimum configuration for testing:**

```ini
[native]
bind = 0.0.0.0:8181
bind = [::]:8181
connections = 10

[bitcoind]
bind = 0.0.0.0:8332
bind = [::]:8332
connections = 10

[electrum]
bind = 0.0.0.0:50001
bind = [::]:50001
connections = 10
```

### 3. Synced Blockchain

Tests use reference blocks and transactions from the Bitcoin blockchain. Your node should be synced at least to:
- **Block height 9000** (minimum for basic tests)
- **Block height 481,824+** (required for SegWit/witness tests — first enforced SegWit block)
- **Block height 500,000+** (recommended for comprehensive testing)

## Quick Start

### Run All Tests (Default Configuration)

```bash
# From test/endpoints/ directory
pytest

# Or from project root
pytest test/endpoints/
```

This uses default localhost endpoints:
- Native: `http://localhost:8181`
- bitcoind RPC: `http://localhost:8332`
- Electrum: `localhost:50001`

### Run Specific Interface Tests

```bash
# Test only native REST
pytest test_native.py

# Test only bitcoind RPC
pytest test_bitcoind_rpc.py

# Test only Electrum protocol
pytest test_electrum.py
```

### Run with Verbose Output

```bash
pytest -v test_native.py
pytest -v --tb=short  # Shorter tracebacks
pytest -vv  # Very verbose
```

## Configuration Options

All tests accept command-line parameters to specify target hosts and ports.

### Native / REST Options

```bash
pytest test_native.py \
  --native-host=192.168.1.100 \
  --native-port=8181
```

**Available options:**
- `--native-host` - Host for native/REST (default: localhost)
- `--native-port` - Port for native/REST (default: 8181)

### bitcoind RPC Options

```bash
pytest test_bitcoind_rpc.py \
  --bitcoind-rpc-host=localhost \
  --bitcoind-rpc-port=8332
```

**With authentication (for Bitcoin Core):**

```bash
pytest test_bitcoind_rpc.py \
  --bitcoind-auth \
  --bitcoind-cookie=/path/to/.cookie
```

**Available options:**
- `--bitcoind-rpc-host` - Host for bitcoind RPC (default: localhost)
- `--bitcoind-rpc-port` - Port for bitcoind RPC (default: 8332)
- `--bitcoind-auth` - Enable authentication (cookie file)
- `--bitcoind-cookie` - Path to cookie file (default: /mnt/core/.cookie)

### Electrum Protocol Options

```bash
pytest test_electrum.py \
  --electrum-host=localhost \
  --electrum-port=50001
```

**Test against public Electrum server:**

```bash
pytest test_electrum.py \
  --electrum-host=fulcrum.sethforprivacy.com \
  --electrum-port=50001
```

**Available options:**
- `--electrum-host` - Host for Electrum protocol (default: localhost)
- `--electrum-port` - Port for Electrum protocol (default: 50001)

### General Options

```bash
# Set custom timeout for all requests
pytest --timeout=60

# Combine multiple options
pytest test_native.py \
  --native-host=192.168.1.100 \
  --native-port=8181 \
  --timeout=30
```

## Test Organization

### test_native.py - Native REST

Tests the `/v1/` HTTP REST endpoints for blockchain exploration.

**Coverage:**
- ✅ General endpoints (configuration, top)
- ✅ Block queries (by hash/height, headers, details, transactions)
- ✅ Block filters (BIP 158/157 bloom filters)
- ✅ Transaction queries (by hash, details, fees)
- ✅ Input/output queries (scripts, witness data, spenders)
- ✅ Address queries (balance, confirmed/unconfirmed)
- ✅ Error handling (404, invalid parameters)

**Example test runs:**

```bash
# Run all native tests
pytest test_native.py

# Run only block-related tests
pytest test_native.py -k "block"

# Run only transaction tests
pytest test_native.py -k "tx"

# Skip slow address tests
pytest test_native.py -k "not address"
```

### test_bitcoind_rpc.py - bitcoind RPC Compatibility

Tests JSON-RPC 2.0 interface compatible with Bitcoin Core RPC.

**Coverage:**
- ✅ Blockchain methods (17 methods)
  - getbestblockhash, getblock, getblockchaininfo, getblockcount
  - getblockfilter, getblockhash, getblockheader, getblockstats
  - getchaintxstats, getchainwork, gettxout, gettxoutsetinfo
  - pruneblockchain, savemempool, scantxoutset
  - verifychain, verifytxoutset
- ✅ Error handling
- ⏭️ Control methods (commented out - 6 methods)
- ⏭️ Mining methods (commented out - 4 methods)
- ⏭️ Network methods (commented out - 9 methods)
- ⏭️ Raw transaction methods (commented out - 9 methods)

**Example test runs:**

```bash
# Test against libbitcoin-server
pytest test_bitcoind_rpc.py

# Test against Bitcoin Core (with auth)
pytest test_bitcoind_rpc.py \
  --bitcoind-rpc-port=9333 \
  --bitcoind-auth \
  --bitcoind-cookie=/path/to/.cookie

# Run only getblock* methods
pytest test_bitcoind_rpc.py -k "getblock"
```

### test_electrum.py - Electrum Protocol

Tests Electrum Protocol 1.4.2 JSON-RPC over TCP.

**Coverage:**
- ✅ Server methods (version, banner, features, ping)
- ✅ Blockchain methods (headers, estimatefee, relayfee)
- ✅ Scripthash methods (balance, history, mempool, listunspent, subscribe)
- ✅ Transaction methods (get, id_from_pos)
- ✅ Mempool methods (fee_histogram)

**Example test runs:**

```bash
# Test against local Electrum server
pytest test_electrum.py

# Test against public server
pytest test_electrum.py \
  --electrum-host=electrum.blockstream.info \
  --electrum-port=50001

# Run only server methods
pytest test_electrum.py -k "server"
```

## Advanced Usage

### Running Specific Tests

```bash
# Run a single test by name
pytest test_native.py::test_configuration

# Run multiple specific tests
pytest test_native.py::test_block_by_hash test_native.py::test_block_by_height

# Run tests matching pattern
pytest test_native.py -k "block_filter"
```

### Pytest Markers

```bash
# Skip marked tests
pytest test_native.py -m "not skip"

# Run only slow tests
pytest test_native.py -m "slow"

# Run parametrized tests with specific parameter
pytest test_native.py -k "filter_type-bloom"
```

### Output Formats

```bash
# Generate JUnit XML report
pytest --junitxml=results.xml

# Generate HTML report (requires pytest-html)
pytest --html=report.html --self-contained-html

# JSON output (requires pytest-json-report)
pytest --json-report --json-report-file=results.json
```

### Continuous Integration

```bash
# CI-friendly output with timestamps
pytest -v --tb=short --durations=10

# Fail fast (stop on first failure)
pytest -x

# Run N tests in parallel (requires pytest-xdist)
pytest -n 4
```

## Test Data Reference

All tests use shared reference data from `utils.ReferenceData`:

| Item | Value | Description |
|------|-------|-------------|
| **GENESIS_HEIGHT** | 0 | Genesis block height |
| **GENESIS_HASH** | 000000000019d668... | Genesis block hash |
| **KNOWN_HEIGHT** | 9000 | Test block height |
| **KNOWN_BLOCK_HASH** | 00000000415c7a0f... | Test block hash (height 9000) |
| **KNOWN_TX_HASH** | a5f71a0b4ccda01f... | Coinbase tx at height 9000 |
| **EXAMPLE_SCRIPTHASH** | 6e8c6c7183154cae... | Test scripthash with activity |
| **SEGWIT_HEIGHT** | 481824 | First SegWit-enforced block (24 Aug 2017) |
| **SEGWIT_BLOCK_HASH** | 0000000000000000001c... | Block hash at height 481824 |
| **SEGWIT_TX_HASH_P2WPKH** | dfcec48bb8491856... | First native P2WPKH tx (witness inputs) |
| **SEGWIT_TX_HASH_P2WPKH2** | f91d0a8a78462bc5... | Second native P2WPKH tx in block 481824 |
| **SEGWIT_TX_HASH_P2WSH** | 461e8a4aa0a0e75c... | Tx with P2WSH output in block 481824 |

> **Note:** Witness data (`input/witness`) requires a SegWit transaction. Tests using `SEGWIT_TX_HASH_P2WPKH` will fail or be skipped if the node is not synced past block **481,824**.

## Troubleshooting

### Common Issues

**1. Connection refused**
```
RuntimeError: RPC connection error: ConnectionRefusedError
```
**Solution:** Ensure libbitcoin-server is running and the endpoint is enabled in configuration.

**2. Timeout errors**
```
requests.exceptions.ReadTimeout: HTTPConnectionPool(...): Read timed out
```
**Solution:** Increase timeout with `--timeout=60` or check server performance.

**3. Test failures on public servers**
```
pytest.xfail: Server error: Method not found
```
**Solution:** Public servers may not implement all methods. This is expected and tests will skip gracefully.

**4. Authentication errors (Bitcoin Core)**
```
RuntimeError: RPC connection error: 401 Unauthorized
```
**Solution:** Use `--bitcoind-auth --bitcoind-cookie=/correct/path/.cookie`

**5. Import errors**
```
ModuleNotFoundError: No module named 'utils'
```
**Solution:** Run pytest from the `test/endpoints/` directory or install package in development mode.

### Debug Mode

```bash
# Enable Python debugger on test failure
pytest --pdb

# Show local variables on failure
pytest -l

# Capture and display stdout/stderr
pytest -s

# Full traceback
pytest --tb=long
```

### Performance Profiling

```bash
# Show 10 slowest tests
pytest --durations=10

# Profile test execution (requires pytest-profiling)
pytest --profile

# Benchmark tests (requires pytest-benchmark)
pytest --benchmark-only
```

## Writing New Tests

### Test Structure Template

```python
def test_new_endpoint(native_config):
    """Test description."""
    # Arrange
    endpoint = "new/endpoint"

    # Act
    data = get_json(native_config["base_url"], endpoint)

    # Assert
    assert isinstance(data, dict)
    assert "expected_field" in data
```

### Using Fixtures

```python
# Use native_config for REST tests
def test_with_config(native_config):
    url = native_config["base_url"]
    host = native_config["host"]
    port = native_config["port"]

# Use bitcoind_rpc_config for RPC tests
def test_rpc_method(bitcoind_rpc_config):
    response = send_rpc(bitcoind_rpc_config, "method_name", [params])

# Use electrum_config for Electrum tests
def test_electrum_method(electrum_config):
    # Connection is managed by fixture
    result = send_rpc("electrum.method", [params])
```

### Parametrized Tests

```python
import pytest

@pytest.mark.parametrize("height,expected_hash", [
    (0, ReferenceData.GENESIS_HASH),
    (9000, ReferenceData.KNOWN_BLOCK_HASH),
])
def test_getblockhash(bitcoind_rpc_config, height, expected_hash):
    response = send_rpc(bitcoind_rpc_config, "getblockhash", [height])
    assert response["result"] == expected_hash
```

## Contributing

When adding new tests:

1. Follow existing code structure and naming conventions
2. Use shared utilities from `utils.py`
3. Add docstrings explaining what each test validates
4. Use appropriate pytest markers (`@pytest.mark.skip`, `@pytest.mark.parametrize`)
5. Handle expected errors gracefully (use `pytest.xfail`, `pytest.skip`)
6. Update this README with new test coverage

## Resources

- [pytest Documentation](https://docs.pytest.org/)
- [libbitcoin-server Documentation](../../docs/)
- [Bitcoin Core RPC Documentation](https://developer.bitcoin.org/reference/rpc/)
- [Electrum Protocol Documentation](https://electrumx.readthedocs.io/en/latest/protocol.html)
- [libbitcoin-server GitHub](https://github.com/libbitcoin/libbitcoin-server)

## License

These tests are part of libbitcoin-server and are licensed under the GNU Affero General Public License v3.0.
