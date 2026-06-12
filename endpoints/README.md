# libbitcoin-server Endpoint Tests

Comprehensive test suite for libbitcoin-server's client-server interfaces.

## Overview

This directory contains Python-based integration tests for all libbitcoin-server network endpoints:

| Interface | Protocol | Test File | Status |
|-----------|----------|-----------|--------|
| **Native REST** | HTTP/S + JSON | `test_native.py` | ✅ Active |
| **bitcoind REST** | HTTP/S + JSON/Binary | `test_bitcoind_rest.py` | 🚧 Planned |
| **bitcoind RPC** | HTTP/S + JSON-RPC 2.0 | `test_bitcoind_rpc.py` | ✅ Active |
| **Electrum** | TCP + JSON-RPC 2.0 | `test_electrum.py` | ✅ Active |
| **Electrum subscriptions** | TCP + push notifications | `test_electrum_subscriptions.py` | ✅ Active |
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
- `--native-host` — Host for native/REST (default: localhost)
- `--native-port` — Port for native/REST (default: 8181)

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
- `--bitcoind-rpc-host` — Host for bitcoind RPC (default: localhost)
- `--bitcoind-rpc-port` — Port for bitcoind RPC (default: 8332)
- `--bitcoind-auth` — Enable authentication (cookie file)
- `--bitcoind-cookie` — Path to cookie file (default: /mnt/core/.cookie)

### Electrum Protocol Options

```bash
pytest test_electrum.py \
  --electrum-host=localhost \
  --electrum-port=50001 \
  --electrum-protocol=1.4:1.6
```

**Test against public Electrum server:**

```bash
pytest test_electrum.py \
  --electrum-host=bitcoin.lukechilds.co \
  --electrum-port=50001
```

**Available options:**

| Option | Default | Description |
|--------|---------|-------------|
| `--electrum-host` | localhost | Electrum server hostname |
| `--electrum-port` | 50001 | Electrum server port |
| `--electrum-protocol` | `1.4:1.6` | Protocol version range offered in the `server.version` handshake. Use `min:max` (e.g. `1.4:1.6`) or a single version (e.g. `1.6`). The server negotiates the highest mutually supported version. |
| `--subscription-timeout` | 60 | Seconds to wait for a push notification (used by `test_electrum_subscriptions.py`) |

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

### test_native.py — Native REST

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
pytest test_native.py
pytest test_native.py -k "block"
pytest test_native.py -k "tx"
pytest test_native.py -k "not address"
```

### test_bitcoind_rpc.py — bitcoind RPC Compatibility

Tests JSON-RPC 2.0 interface compatible with Bitcoin Core RPC.

**Coverage:**
- ✅ Blockchain methods (17 methods)
  - getbestblockhash, getblock, getblockchaininfo, getblockcount
  - getblockfilter, getblockhash, getblockheader, getblockstats
  - getchaintxstats, getchainwork, gettxout, gettxoutsetinfo
  - pruneblockchain, savemempool, scantxoutset
  - verifychain, verifytxoutset
- ✅ Error handling
- ⏭️ Control methods (commented out — 6 methods)
- ⏭️ Mining methods (commented out — 4 methods)
- ⏭️ Network methods (commented out — 9 methods)
- ⏭️ Raw transaction methods (commented out — 9 methods)

**Example test runs:**

```bash
pytest test_bitcoind_rpc.py
pytest test_bitcoind_rpc.py \
  --bitcoind-rpc-port=9333 \
  --bitcoind-auth \
  --bitcoind-cookie=/path/to/.cookie
pytest test_bitcoind_rpc.py -k "getblock"
```

### test_electrum.py — Electrum Protocol

Tests Electrum Protocol JSON-RPC over TCP. All assertions are spec-driven — they
verify the desired protocol behavior rather than mirroring the current server
implementation. Failing tests indicate gaps between spec and implementation.

**Architecture:**
- Single persistent TCP connection for the whole test session
- `server.version` handshake performed once in the session fixture (the protocol requires it to be the first message sent)
- Unique monotonic RPC IDs per request: stale delayed responses are discarded automatically
- Push notifications from active subscriptions filtered out transparently
- `select()`-based buffered I/O instead of `socket.makefile()` — a timeout on any individual response never corrupts the reader or breaks subsequent tests

**Coverage:**

| Category | Tests | Methods |
|----------|-------|---------|
| Server | 7 | `server.version`, `server.banner`, `server.features`, `server.ping`, `server.add_peer`, `server.donation_address`, `server.peers.subscribe` |
| Block header | 3 | `blockchain.block.header` (plain and with cp proof, including protocol doc example) |
| Block headers | 9 | `blockchain.block.headers` (plain, with cp, bulk, edge cases) |
| Headers subscribe | 1 | `blockchain.headers.subscribe` (initial response only) |
| Fee estimation | 2 | `blockchain.estimatefee`, `blockchain.relayfee` |
| Scripthash | 6 | `blockchain.scripthash.get_balance`, `.get_history`, `.get_mempool`, `.listunspent`, `.subscribe`, `.unsubscribe` |
| Transactions | 5 | `blockchain.transaction.broadcast`, `.get`, `.get` (verbose), `.get_merkle`, `.id_from_pos` |
| Mempool | 1 | `mempool.get_fee_histogram` |
| **Total** | **34** | |

**Example test runs:**

```bash
# Local Electrum server (default: localhost:50001, protocol 1.4–1.6)
pytest test_electrum.py

# Negotiate a specific protocol version
pytest test_electrum.py --electrum-protocol=1.6
pytest test_electrum.py --electrum-protocol=1.4:1.4

# Test against a public server
pytest test_electrum.py --electrum-host=bitcoin.lukechilds.co --electrum-port=50001

# Filter by method group
pytest test_electrum.py -k "server"
pytest test_electrum.py -k "scripthash"
pytest test_electrum.py -k "block_header"

# Debug mode (pretty-print every JSON-RPC message)
ELECTRUM_DEBUG=1 pytest test_electrum.py -s -k "block_header"
```

### test_electrum_subscriptions.py — Push Notification Tests

Tests that subscribe to server events and wait for real blockchain push notifications.
Requires a live node with active network traffic and much longer timeouts than the
regular request/response tests. Each test opens its own fresh TCP connection.

```bash
pytest test_electrum_subscriptions.py
pytest test_electrum_subscriptions.py --subscription-timeout=120

# Optional: trigger a block/tx event to avoid waiting the full timeout
ELECTRUM_TRIGGER_HEADERS="bitcoin-cli generatetoaddress 1 <address>" \
  pytest test_electrum_subscriptions.py
```

**Tests:**
- `test_headers_notification` — subscribes to `blockchain.headers.subscribe`, waits for a block notification, validates `{method, params: [{height, hex}]}` format
- `test_scripthash_notification` — subscribes to `blockchain.scripthash.subscribe`, waits for a status-change notification, validates `{method, params: [scripthash, status]}` format

---

## Electrum Protocol Version Reference

The Electrum protocol has evolved across versions. This table shows which methods exist in which version and where behavior differs. The test suite uses version-conditional assertions where applicable, based on the negotiated version returned by the `server.version` handshake.

| Method / Behavior | v1.0 | v1.1 | v1.2 | v1.3 | v1.4 | v1.4.2 | v1.6 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **Server** | | | | | | | |
| `server.version` | ✅ | ✅ | ✅ | ✅ | ✅ ¹ | ✅ ¹ | ✅ ¹ |
| `server.banner` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.features` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.ping` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.add_peer` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.donation_address` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.peers.subscribe` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Block Header** | | | | | | | |
| `blockchain.block.header` | — ² | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.block.header` with `cp_height` | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| → response key: `"header"` (160-char hex) | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| → response key: `"branch"` (list of 64-char hashes) | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| → response key: `"root"` (64-char merkle root) | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Block Headers** | | | | | | | |
| `blockchain.block.headers` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| → response key: `"hex"` (concatenated hex string) | — | — | ✅ | ✅ | ✅ | ✅ | — ³ |
| → response key: `"headers"` (array of 160-char hex) | — | — | — | — | — | — | ✅ ³ |
| `blockchain.block.headers` with `cp_height` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Headers Subscribe** | | | | | | | |
| `blockchain.headers.subscribe` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| → deserialized: `{block_height, version, prev_block_hash, …}` | ✅ | ✅ | ✅ | — | — | — | — |
| → serialized raw: `{height, hex}` (hex = 160 chars) | — | — | — | ✅ | ✅ | ✅ | ✅ |
| **Fee Estimation** | | | | | | | |
| `blockchain.estimatefee` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.relayfee` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ ⁴ |
| **Scripthash** | | | | | | | |
| `blockchain.scripthash.get_balance` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.get_history` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.get_mempool` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.listunspent` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.subscribe` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.unsubscribe` | — | — | — | — | ✅ | ✅ | ✅ |
| **Transactions** | | | | | | | |
| `blockchain.transaction.broadcast` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.get` (raw hex) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.get` (verbose) | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.get_merkle` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.id_from_pos` | — | — | — | — | ✅ | ✅ | ✅ |
| **Mempool** | | | | | | | |
| `mempool.get_fee_histogram` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |

**Legend:** ✅ available and expected to work · — not yet introduced in this version · ✗ removed

**Footnotes:**

¹ Since v1.4, `server.version` must be the **first** message sent by the client. Any other message sent first results in a protocol error. The test suite performs the handshake as the session fixture before any test runs.

² v1.0 used `blockchain.block.get_header` (different method name). `blockchain.block.header` was introduced in v1.1.

³ In v1.6, `blockchain.block.headers` changed its data format: the `"hex"` key (a single concatenated hex string of all headers) was replaced by `"headers"` (an array of individual 160-char hex strings). The test suite accepts either key, determined by which one is present in the response.

⁴ `blockchain.relayfee` was removed in v1.6. Clients should read `server.features["fee_rate"]` instead. When `negotiated_version >= "1.6"`, the test verifies that the server rejects the call with any error (libbitcoin-server returns `wrong_version`; ElectrumX returns `-32601 unknown method` — both are valid rejections).

⁵ v1.0 used `blockchain.address.*` (address-based methods). Scripthash-based methods (`blockchain.scripthash.*`) were introduced in v1.1.

### Version-Conditional Test Assertions

The following tests adjust their assertions based on `negotiated_version` from the `server.version` handshake:

| Test | Condition | Expected behavior |
|------|-----------|-------------------|
| `test_blockchain_headers_subscribe` | `< "1.3"` | Response has `block_height` (int) and at least one of `version`, `prev_block_hash`, `merkle_root` |
| `test_blockchain_headers_subscribe` | `>= "1.3"` | Response has `height` (int) and `hex` (160-char string) |
| `test_blockchain_relayfee` | `< "1.6"` | Returns a positive `float` (BTC/kB) |
| `test_blockchain_relayfee` | `>= "1.6"` | Server must reject with any error; test passes if `ValueError` is raised |
| `test_blockchain_block_headers` (all variants) | `< "1.6"` | Result dict has `"hex"` key (concatenated string, length = `count × 160`) |
| `test_blockchain_block_headers` (all variants) | `>= "1.6"` | Result dict has `"headers"` key (array, each element 160 chars) |

### Known Deviations from Spec

These are the four `⚠️ xfail` outcomes when running against libbitcoin-server. Each is documented here so the expected test result is not mistaken for a regression.

| Test | Server response | Spec-correct behavior | Status |
|------|-----------------|-----------------------|--------|
| `test_server_add_peer` | `not_implemented` | Must return `bool` | Gap — peer list management not implemented |
| `test_blockchain_block_headers_count_zero` | `not_found` error | Should return `{count: 0, hex: ""}` | Gap — count=0 is not prohibited by the spec |
| `test_blockchain_block_headers_cp_target_overflow` | `target_overflow` error | Error is correct — `cp_height < target` is invalid | By design — the test triggers this deliberately |
| `test_blockchain_transaction_broadcast` | error (invalid tx) | Error is correct — 10-byte payload is not a valid transaction | By design — the test exercises the broadcast rejection path |

---

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

# Run subscription tests
pytest test_electrum_subscriptions.py
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

---

## Test Data Reference

All tests use shared reference data from `utils.ReferenceData`:

| Constant | Value | Description |
|----------|-------|-------------|
| `GENESIS_HEIGHT` | 0 | Genesis block height |
| `GENESIS_HASH` | `000000000019d668…` | Genesis block hash |
| `KNOWN_HEIGHT` | 9000 | Test block height |
| `KNOWN_BLOCK_HASH` | `00000000415c7a0f…` | Test block hash (height 9000) |
| `KNOWN_TX_HASH` | `a5f71a0b4ccda01f…` | Coinbase tx at height 9000 |
| `EXAMPLE_SCRIPTHASH` | `6e8c6c7183154cae…` | Test scripthash with confirmed activity |
| `SEGWIT_HEIGHT` | 481824 | First SegWit-enforced block (24 Aug 2017) |
| `SEGWIT_BLOCK_HASH` | `0000000000000000001c…` | Block hash at height 481824 |
| `SEGWIT_TX_HASH_P2WPKH` | `dfcec48bb8491856…` | First native P2WPKH tx (witness inputs) |
| `SEGWIT_TX_HASH_P2WPKH2` | `f91d0a8a78462bc5…` | Second native P2WPKH tx in block 481824 |
| `SEGWIT_TX_HASH_P2WSH` | `461e8a4aa0a0e75c…` | Tx with P2WSH output in block 481824 |

> **Note:** Witness data (`input/witness`) requires a SegWit transaction. Tests using `SEGWIT_TX_HASH_P2WPKH` will fail or be skipped if the node is not synced past block **481,824**.

---

## Troubleshooting

### Common Issues

**1. Connection refused**
```
RuntimeError: RPC connection error: ConnectionRefusedError
```
Ensure libbitcoin-server is running and the endpoint is enabled in configuration.

**2. Timeout errors**
```
requests.exceptions.ReadTimeout: HTTPConnectionPool(...): Read timed out
```
Increase timeout with `--timeout=60` or check server performance.

**3. Tests skip after a timeout on a previous test**

The Electrum test suite uses `select()`-based I/O on a module-level byte buffer. A timeout on one test does **not** corrupt the socket reader — the buffer is intact and subsequent tests continue normally. If you see cascading skips, the server likely closed the connection (check server logs for `unexpected method` or `channel stop`).

**4. `unexpected method` in server log / cascading skips**

libbitcoin-server closes the TCP connection when it receives an unknown method name. Ensure the method name is spelled exactly as in the Electrum spec — notably `blockchain.scripthash.listunspent` (no underscore between `list` and `unspent`), unlike the underscore convention used by `get_balance` / `get_history`.

**5. Test failures on public servers**
```
SKIP: No response (possibly unsupported method)
```
Some public servers (electrs in particular) do not implement all Electrum methods and simply do not respond. The test skips cleanly. This is not a test failure.

**6. Authentication errors (Bitcoin Core)**
```
RuntimeError: RPC connection error: 401 Unauthorized
```
Use `--bitcoind-auth --bitcoind-cookie=/correct/path/.cookie`

**7. Import errors**
```
ModuleNotFoundError: No module named 'utils'
```
Run pytest from the `test/endpoints/` directory or install package in development mode.

### Debug Output

Each test module supports a debug environment variable that enables pretty-printed JSON logging of every request and response:

| Test file | Variable | Output |
|-----------|----------|--------|
| `test_electrum.py` | `ELECTRUM_DEBUG=1` | `>>>` JSON-RPC payload / `<<<` response with elapsed time |
| `test_native.py` | `NATIVE_DEBUG=1` | `>>> GET <url>` / `<<<` response JSON with elapsed time |
| `test_bitcoind_rpc.py` | `BITCOIND_DEBUG=1` | `>>>` JSON-RPC payload / `<<<` response with elapsed time |

```bash
# Electrum — pretty-print all requests and responses
ELECTRUM_DEBUG=1 pytest test_electrum.py -s

# Combine with -k to focus on a single test
ELECTRUM_DEBUG=1 pytest test_electrum.py -s -k "block_headers_20000"
```

> Use `-s` (or `--capture=no`) together with the debug variable so pytest does not suppress stdout.

### Performance Profiling

```bash
# Show 10 slowest tests
pytest --durations=10

# Profile test execution (requires pytest-profiling)
pytest --profile
```

---

## Writing New Tests

### Electrum Test Structure

Electrum tests share a single persistent connection. Use `send_rpc()` for all calls inside tests — it handles connection errors (skip), server errors (xfail), and stale responses automatically.

```python
def test_new_electrum_method():
    result = send_rpc("some.method", [param1, param2])
    assert isinstance(result, dict)
    assert "expected_field" in result
```

For version-conditional assertions:

```python
def test_version_dependent():
    result = send_rpc("blockchain.headers.subscribe")
    if negotiated_version is not None and negotiated_version >= "1.3":
        assert "height" in result and "hex" in result
    else:
        assert "block_height" in result
```

For methods that must be rejected in a specific version, bypass `send_rpc()` and use `_rpc_raw()` directly so you can catch the `ValueError`:

```python
def test_method_removed_in_v16():
    if negotiated_version is not None and negotiated_version >= "1.6":
        try:
            _rpc_raw("some.removed.method")
            pytest.fail("Method should be rejected in v1.6+")
        except ValueError:
            pass
        return
    result = send_rpc("some.removed.method")
    assert result > 0
```

### Native REST Test Structure

```python
def test_new_endpoint(native_config):
    data = get_json(native_config["base_url"], "new/endpoint")
    assert isinstance(data, dict)
    assert "expected_field" in data
```

### Fixtures

| Fixture | Scope | Use for |
|---------|-------|---------|
| `native_config` | session | Native REST tests — provides `base_url`, `host`, `port` |
| `bitcoind_rpc_config` | session | bitcoind RPC tests — provides `url`, `auth`, `timeout` |
| `electrum_config` | session | Electrum tests — provides `host`, `port`, `protocol`, `timeout` |
| `persistent_connection` | session | Electrum session socket — autouse, performs handshake |

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

---

## Contributing

When adding new tests:

1. Follow existing code structure and naming conventions
2. Use shared utilities from `utils.py`
3. Assert the **spec-required** behavior, not just what the current server returns — failing tests are how we find implementation gaps
4. Use `pytest.xfail` (via `send_rpc`) for errors that are correct by design; use `pytest.skip` for missing prerequisites
5. Handle version-conditional behavior with `negotiated_version` comparisons
6. Update the Cross-Server Compatibility table in this README after verifying against multiple servers

## Resources

- [pytest Documentation](https://docs.pytest.org/)
- [libbitcoin-server Documentation](../../docs/)
- [Bitcoin Core RPC Documentation](https://developer.bitcoin.org/reference/rpc/)
- [Electrum Protocol Documentation](https://electrumx.readthedocs.io/en/latest/protocol.html)
- [Electrum Protocol Changelog](https://electrumx.readthedocs.io/en/latest/protocol-changes.html)
- [libbitcoin-server GitHub](https://github.com/libbitcoin/libbitcoin-server)

## License

These tests are part of libbitcoin-server and are licensed under the GNU Affero General Public License v3.0.
