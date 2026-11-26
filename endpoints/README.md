# libbitcoin-server Endpoint Tests

Integration tests for the client-server interfaces of libbitcoin-server
(`bs`), run with pytest against a running server.

## Overview

| Interface | Protocol | Test file | Tests |
|-----------|----------|-----------|------:|
| **Smoke** (any network) | HTTP pages, heights, node log | `test_smoke.py` | 10 |
| **Native REST** | HTTP + JSON | `test_native.py` | 46 |
| **Esplora REST** | HTTP + JSON, websocket | `test_esplora.py` | 49 |
| **bitcoind REST** | HTTP + JSON/hex/binary | `test_bitcoind_rest.py` | 15 |
| **bitcoind RPC** | HTTP + JSON-RPC | `test_bitcoind_rpc.py` | 38 |
| **btcd** | websocket/HTTP + JSON-RPC 1.0 | `test_btcd_rpc.py` | 43 |
| **Electrum** | TCP + JSON-RPC 2.0 | `test_electrum.py` | 34 |
| **Electrum subscriptions** | TCP + notifications | `test_electrum_subscriptions.py` | 2 |
| **Sparrow** | TCP + JSON-RPC 2.0 | `test_sparrow.py` | 13 |
| **Transports** | TCP downgrade, HTTP POST, websocket | `test_transports.py` | 6 |
| **Stratum v1** | TCP/HTTP/websocket + JSON-RPC 1.0 | `test_stratum_v1.py` | 45 |
| **Robustness** (all services) | raw TCP/HTTP/websocket, misbehaving clients | `test_robustness_*.py`, `test_longrun.py` | 145 |
| **Stratum v2** | websocket + binary frames | `test_stratum_v2.py` | 37 |
| **Stratum v2 Noise** (no server) | fixed-key handshake and transport vectors | `test_sv2_noise.py` | 13 |

Test counts are test functions; parametrized cases run more (about 2,150 in
all).

## Prerequisites

### 1. Python dependencies

```bash
pip install pytest requests websocket-client
```

### 2. A running server

Start `bs` with the endpoints under test bound. See
[configuration.md](../docs/configuration.md) and `bs --settings` for all
settings. A service is off unless it has a `bind` and a nonzero `connections`.

```ini
[native]
bind = 127.0.0.1:8181
connections = 32

[esplora]
bind = 127.0.0.1:3000
connections = 32

[bitcoind]
bind = 127.0.0.1:8332
connections = 32
enable_utxo = true          # gettxoutsetinfo, scantxoutset
# credential = user:password

[btcd]
bind = 127.0.0.1:8334
connections = 32
# credential = user:password

[electrum]
bind = 127.0.0.1:50001
connections = 32

[sparrow]
bind = 127.0.0.1:50003
connections = 32

[admin]
bind = 127.0.0.1:8080
connections = 32

[stratum_v1]
bind = 127.0.0.1:8880
connections = 32

[stratum_v2]
bind = 127.0.0.1:8980
connections = 32
```

### 3. Chain data

The reference data (`utils.ReferenceData`) is mainnet. Tests that need a
height, a current node or an open pool are marked and skip themselves when
the node does not meet the need (see [Markers](#markers)):

- **height 9,000**: basic block and transaction tests
- **height 481,824**: SegWit tests (first block enforcing SegWit)
- **current** (not in initial block download): address activity, pool tests

`test_smoke.py`, `test_stratum_v1.py` and `test_stratum_v2.py` work on any
network. The stratum share and block tests need regtest (see
[test_stratum_v1.py](#test_stratum_v1py--stratum-v1) and
[test_stratum_v2.py](#test_stratum_v2py--stratum-v2)).

## Quick start

```bash
cd endpoints

# Everything at the default localhost ports
pytest

# One interface
pytest test_native.py
pytest test_bitcoind_rpc.py --bitcoind-auth --bitcoind-cookie=/path/to/.cookie
pytest test_btcd_rpc.py --btcd-username=user --btcd-password=pass
pytest test_electrum.py
pytest test_stratum_v1.py --stratum-v1-worker=<address>
pytest test_stratum_v2.py --stratum-v2-user=<address>

# Verbose, short tracebacks, skip and xfail reasons
pytest -v --tb=short -rsx test_native.py
```

## Options

All options have localhost defaults.

| Option | Default | Use |
|--------|---------|-----|
| `--native-host`, `--native-port` | localhost, 8181 | native REST |
| `--esplora-host`, `--esplora-port` | localhost, 3000 | Esplora REST |
| `--admin-host`, `--admin-port` | localhost, 8080 | admin pages |
| `--bitcoind-rpc-host`, `--bitcoind-rpc-port` | localhost, 8332 | bitcoind JSON-RPC |
| `--bitcoind-rest-host`, `--bitcoind-rest-port` | localhost, 8332 | bitcoind REST |
| `--bitcoind-auth` | off | send the credential of `--bitcoind-cookie` (bitcoind RPC, REST and smoke suites) |
| `--bitcoind-cookie` | /mnt/core/.cookie | file holding `user:password` |
| `--bitcoind-username`, `--bitcoind-password` | none | the bitcoind credential for the robustness suites |
| `--btcd-host`, `--btcd-port` | localhost, 8334 | btcd |
| `--btcd-username`, `--btcd-password` | none | the btcd credential (basic authorization and ws `authenticate`); without it the credential tests skip |
| `--electrum-host`, `--electrum-port` | localhost, 50001 | Electrum |
| `--electrum-protocol` | 1.4:1.6 | versions offered in `server.version` (`min:max` or one version) |
| `--sparrow-host`, `--sparrow-port` | localhost, 50003 | Sparrow |
| `--stratum-v1-host`, `--stratum-v1-port` | localhost, 8880 | Stratum v1 |
| `--stratum-v1-worker` | worker | the worker name: an address (`<address>[.<rig>]`) unless the server configures `payout_address` |
| `--stratum-v1-password` | none | the stratum credential password, when `[stratum_v1] credential` is configured (its user is the worker account) |
| `--stratum-v2-host`, `--stratum-v2-port` | localhost, 8980 | Stratum v2 (websocket) |
| `--stratum-v2-user` | worker | the user identity: an address (`<address>[.<rig>]`) unless the server configures `payout_address` |
| `--stratum-v2-password` | none | the stratum credential password, when `[stratum_v2] credential` is configured (its user is the identity account) |
| `--timeout` | 30 | request timeout in seconds |
| `--subscription-timeout` | 60 | seconds to wait for a notification |
| `--scan-timeout` | 1800 | seconds for whole-set scans (`gettxoutsetinfo`, `scantxoutset`) |
| `--run-slow` | off | include tests marked `slow` (they wait on a real event) |
| `--log` | none | node log (e.g. `console.log`) the smoke suite checks for faults since startup |
| `--robust-services` | native,electrum,btcd | services the robustness suites attack and check |
| `--robust-connections` | 32 | the `connections` limit configured on those services |
| `--bs-pid` | none | server process id on this host: enables the process and CPU checks |
| `--no-spin-check` | off | skip the pegged-thread check (while the node syncs) |
| `--run-shutdown` | off | run the `test_longrun.py` test that stops the server |

### Credentials

`--bitcoind-auth` reads the bitcoind credential from a file. A configured
`[bitcoind] credential = user:password` works the same way when the file
holds `user:password`. The robustness suites take it separately, as
`--bitcoind-username` and `--bitcoind-password`. A server with a bitcoind
credential therefore needs both:

```bash
echo "user:password" > bitcoind.cookie
pytest --bitcoind-auth --bitcoind-cookie=bitcoind.cookie \
  --bitcoind-username=user --bitcoind-password=password ...
```

Without them the bitcoind requests get `401 Unauthorized`, and every
robustness case errors in its survival check.

## Markers

| Marker | Effect |
|--------|--------|
| `slow` | skipped unless `--run-slow` (waits on a real block or timer) |
| `requires_height(h)` | skipped unless the native top is at least `h` |
| `requires_current` | skipped while bitcoind `getblockchaininfo` reports initial block download |
| `requires_pool` | skipped unless native `/v1/fees` answers (the pool is open) |

```bash
pytest -m "not slow"
pytest --run-slow test_btcd_rpc.py --subscription-timeout=120
```

## Test files

### test_smoke.py — Smoke

For any running server on any network: the native and admin pages and their
assets are served (and revalidated), a malformed native request is refused,
the native heights are ordered, Esplora and bitcoind agree with native on the
height, and with `--log` the node log shows no fault since startup.

```bash
pytest test_smoke.py --native-port=8181 --admin-port=8080 --log=console.log
```

### test_native.py — Native REST

The `/v1/` REST interface: configuration, tops, blocks (by hash and height,
headers, header context, details, txs, txs details, tx by position, scripts),
block filters, transactions (header, details, inputs, outputs, scripts,
witness, spenders), addresses (all, confirmed, unconfirmed, balance), block
templates, and errors (unknown hash, invalid height, unsupported format).

```bash
pytest test_native.py -k "block"
NATIVE_DEBUG=1 pytest test_native.py -s -k top
```

### test_esplora.py — Esplora REST

The Esplora (Blockstream explorer) API: tip, blocks, block txs and txids,
transactions (hex, raw, status, outspends, merkle proofs), broadcast and
package broadcast errors, addresses and script hashes (txs, paging, pool,
utxo), the pool (`/mempool`, `/mempool/txids`, `/mempool/recent`), fee
estimates, invalid targets, and the websocket interface.

```bash
pytest test_esplora.py --esplora-port=3000
```

### test_bitcoind_rest.py — bitcoind REST

The bitcoind REST endpoints under `/rest/`: chaininfo, block,
block/notxdetails, spenttxouts, blockhashbyheight, headers, blockpart and the
basic filters, as `.json`, `.hex` and `.bin`. A non-200 status degrades to
xfail. With a bitcoind credential, pass `--bitcoind-auth`.

### test_bitcoind_rpc.py — bitcoind RPC

bitcoind JSON-RPC. `bs` implements 81 methods. The wallet and test groups,
and 15 other methods (such as `pruneblockchain`, `setban`, `stop`), are
answered with an error. The suite covers 22 of the 81:

| Group | Tested | Methods not yet tested |
|-------|-------:|------------------------|
| Blockchain | 14/33 | `getchainstates`, `getchaintips`, `getdeploymentinfo`, `getdescriptoractivity`, `getdifficulty`, `getmempoolancestors`, `getmempooldescendants`, `getmempoolentry`, `getmempoolinfo`, `getrawmempool`, `gettxoutproof`, `gettxspendingprevout`, `importmempool`, `preciousblock`, `scanblocks`, `verifytxoutproof`, `waitforblock`, `waitforblockheight`, `waitfornewblock` |
| Mining | 5/5 | — |
| Raw transactions | 2/15 | `analyzepsbt`, `combinepsbt`, `combinerawtransaction`, `converttopsbt`, `createpsbt`, `createrawtransaction`, `decodepsbt`, `decoderawtransaction`, `finalizepsbt`, `joinpsbts`, `submitpackage`, `testmempoolaccept`, `utxoupdatepsbt` |
| Network | 1/12 | `addnode`, `disconnectnode`, `exportasmap`, `getaddednodeinfo`, `getaddrmaninfo`, `getconnectioncount`, `getnettotals`, `getnodeaddresses`, `getpeerinfo`, `ping`, `setnetworkactive` |
| Utility | 0/8 | `createmultisig`, `decodescript`, `deriveaddresses`, `estimatesmartfee`, `getdescriptorinfo`, `getindexinfo`, `validateaddress`, `verifymessage` |
| Control | 0/6 | `getmemoryinfo`, `getopenrpcinfo`, `getrpcinfo`, `help`, `logging`, `uptime` |
| ZMQ, version | 0/2 | `getzmqnotifications`, `negotiateversion` |

Tested:
- **Blockchain:** `getbestblockhash`, `getblock`, `getblockchaininfo`,
  `getblockcount`, `getblockfilter`, `getblockhash`, `getblockheader`,
  `getblockstats`, `getchaintxstats`, `gettxout`, `gettxoutsetinfo`,
  `savemempool`, `scantxoutset`, `verifychain`, and `pruneblockchain` (an
  error)
- **Mining:** `getmininginfo` (top and next block work), `getnetworkhashps`
  (default window, genesis, invalid window), `getblocktemplate` (BIP145
  segwit rule and template mode required, BIP22 fields, BIP141 commitment,
  transaction dependencies), `submitblock` (duplicate, undecodable, invalid
  with a reject reason), `submitheader` (known, unknown parent,
  undecodable), and `getprioritisedtransactions`/`prioritisetransaction`
  (`-33`). On regtest a block is built from the template, mined and
  submitted, and becomes the top.
- **Raw transactions:** `getrawtransaction` (raw, verbose, coinbase, SegWit,
  unknown), `sendrawtransaction` (invalid hex, malformed, and with
  `BITCOIND_ALLOW_BROADCAST=1` a known transaction)
- **Network:** `getnetworkinfo`

A valid error response to `send_rpc` degrades to xfail. Error paths use
`raw_rpc` and assert the code.

`gettxoutsetinfo`, `scantxoutset` and `verifychain` scan the whole set and
take minutes on mainnet (`--scan-timeout`). `gettxoutsetinfo` occupies the
shared thread pool while it runs, so do not run it beside other long tests.

```bash
pytest test_bitcoind_rpc.py --bitcoind-auth --bitcoind-cookie=/path/to/.cookie
BITCOIND_DEBUG=1 pytest test_bitcoind_rpc.py -s -k getblock
```

### test_btcd_rpc.py — btcd

The btcd-compatible endpoint: JSON-RPC 1.0 over a persistent websocket (the
session and notification methods) and over HTTP POST (the methods shared with
bitcoind).

- **Session:** `authenticate` (no credential, credential match, mismatch),
  `session`, `getcurrentnet`
- **Notifications:** `notifyblocks`/`stopnotifyblocks`, a real
  `blockconnected` notification (slow), `notifynewtransactions`,
  `notifyreceived`, `notifyspent` and their `stop*` methods, valid and
  invalid addresses and outpoints
- **Filters:** `loadtxfilter`, `rescanblocks`, `filteredblockconnected`
- **`rescan`:** unknown begin block rejected, empty address and outpoint
  lists finish at once with `rescanfinished`, addresses not implemented
- **Methods:** `getdifficulty`, `getinfo` (also over HTTP POST),
  `getnettotals`, `getnetworkhashps`, `createrawtransaction`/
  `decoderawtransaction`, `decodescript`, `validateaddress`, `help`, `stop`
  (not implemented), bitcoind methods over both transports on one websocket
- **Envelope:** the id is echoed, and an unknown method is an error that
  keeps the connection

```bash
pytest test_btcd_rpc.py --btcd-username=user --btcd-password=pass
BTCD_DEBUG=1 pytest test_btcd_rpc.py -s -k session

# blockconnected waits for a real block; trigger one instead (regtest)
BTCD_TRIGGER_BLOCK="bitcoin-cli generatetoaddress 1 <address>" \
  pytest test_btcd_rpc.py --run-slow -k blockconnected
```

### test_electrum.py — Electrum

The Electrum protocol over TCP, asserting the specified behavior. The suite
uses one connection for the session, sends `server.version` first (the
protocol requires it), uses a new id per request, and sets notifications
aside.

| Category | Tests | Methods |
|----------|------:|---------|
| Server | 7 | `server.version`, `server.banner`, `server.features`, `server.ping`, `server.add_peer`, `server.donation_address`, `server.peers.subscribe` |
| Block header | 3 | `blockchain.block.header` (with and without `cp_height`) |
| Block headers | 9 | `blockchain.block.headers` (with and without `cp_height`, edge cases) |
| Headers subscribe | 1 | `blockchain.headers.subscribe` |
| Fees | 2 | `blockchain.estimatefee`, `blockchain.relayfee` |
| Script hash | 6 | `blockchain.scripthash.get_balance`, `.get_history`, `.get_mempool`, `.listunspent`, `.subscribe`, `.unsubscribe` |
| Transactions | 5 | `blockchain.transaction.broadcast`, `.get` (raw and verbose), `.get_merkle`, `.id_from_pos` |
| Pool | 1 | `mempool.get_fee_histogram` |

```bash
pytest test_electrum.py --electrum-protocol=1.6
pytest test_electrum.py --electrum-host=electrum.example --electrum-port=50001
ELECTRUM_DEBUG=1 pytest test_electrum.py -s -k block_header
```

See [Electrum protocol versions](#electrum-protocol-versions) for the
version-dependent assertions.

### test_electrum_subscriptions.py — Electrum notifications

Each test opens its own connection, subscribes, and waits up to
`--subscription-timeout` for a real notification: a new block
(`blockchain.headers.subscribe`) and a script hash status change
(`blockchain.scripthash.subscribe`).

```bash
pytest test_electrum_subscriptions.py --subscription-timeout=900
ELECTRUM_TRIGGER_HEADERS="bitcoin-cli generatetoaddress 1 <address>" \
  pytest test_electrum_subscriptions.py
```

`ELECTRUM_TRIGGER_SCRIPTHASH` triggers the script hash event the same way.

### test_sparrow.py — Sparrow

The Sparrow service, on its own binding: the Electrum interface plus
`blockchain.block.stats` and `blockchain.silentpayments.subscribe`/
`unsubscribe`, and the `silent_payments` field of `server.features`, which
the plain Electrum service omits. `blockchain.block.stats` is a stub, and its
two tests are xfail until it is bound to the store. The suite skips when no
Sparrow service is bound.

### test_transports.py — Transports

Each JSON-RPC service is served on one channel over three transports: raw
TCP (a JSON request downgrades the connection from HTTP), HTTP POST, and
websocket. These tests reach bitcoind and btcd over TCP, and Electrum over
HTTP POST and websocket. A raw TCP request carries no HTTP authorization, so
the downgrade tests skip where a credential is configured.

### test_stratum_v1.py — Stratum v1

The mining interface: JSON-RPC 1.0 replies (no `jsonrpc` member) with
positional errors `[code, message, data]`.

- **`mining.subscribe`:** result shape, a distinct extranonce1 per
  connection, the parameters miners send (session id, host, port, nulls),
  HTTP POST, and a websocket that is notified jobs like TCP
- **`mining.authorize`:** invalid address, null password, a `.rig` suffix,
  a second worker paying elsewhere rejected, and with a credential: wrong
  password rejected, `mining.submit` refused before it (`107`), and HTTP
  basic authorization
- **`mining.configure`:** version rolling (BIP310) masks granted within
  `1fffe000`, other extensions declined
- **Unserved methods:** `mining.get_transactions` and `mining.multi_version`
  answer `20`, `mining.extranonce.subscribe` is acknowledged
- **Jobs:** field formats, `mining.set_difficulty` before the first
  `mining.notify`, the previous hash is the confirmed top, and the coinbase:
  BIP34 height, extranonce push, BIP54 sequence and locktime, payout to the
  worker address, BIP141 commitment
- **Difficulty:** `mining.suggest_difficulty` re-notifies the job at the new
  difficulty, floored at the minimum, `mining.suggest_target`, and decay
  without shares (slow, about 40 s)
- **`mining.submit`:** not subscribed (`25`), not authorized and another
  worker (`24`), unknown job (`21`), malformed fields and timestamps the
  header rejects (`20`), version bits outside the mask (`20`), low difficulty
  (`23`), a valid share answered once (`22`), and a mined block that becomes
  the top, followed by a clean job

The share and block tests build the coinbase, merkle root and header from the
job as a miner does, and search nonces in python, so they run only where the
targets are reachable: a small `minimum_difficulty` for shares, and regtest
for blocks (otherwise they skip). Without a stratum service the suite skips.

A regtest node for the full suite (`bs -n regtest`, one-time `-m` to make the
store):

```ini
[outbound]
connections = 0
host_pool_capacity = 0

[node]
currency_window_minutes = 0   # serve while not current

[native]
bind = 127.0.0.1:19181
connections = 16

[stratum_v1]
bind = 127.0.0.1:18880
connections = 16
minimum_difficulty = 0.00001  # shares reachable in python
coinbase_tag = /libbitcoin-test/
# payout_address = bcrt1q...  # any worker name is then authorized
# credential = miner:secret   # then --stratum-v1-worker=miner.rig1 --stratum-v1-password=secret
```

```bash
bs -n regtest -c regtest.cfg -m
bs -n regtest -c regtest.cfg -a
pytest test_stratum_v1.py --run-slow --stratum-v1-port=18880 \
  --native-port=19181 \
  --stratum-v1-worker=bcrt1qyg3zyg3zyg3zyg3zyg3zyg3zyg3zyg3zs4w3j0
```

Each run mines blocks on the regtest chain.

### test_stratum_v2.py — Stratum v2

The mining protocol over websocket, one binary frame to each message (no
Noise). The suite carries its own frame codec.

- **SetupConnection:** version 2 within the offered range, the standard jobs
  and version rolling flags accepted, work selection and other protocols
  refused (and the connection dropped), mining before setup dropped, and a
  GET without upgrade refused
- **OpenExtendedMiningChannel:** result fields, a future first job followed
  by its previous block (the confirmed top), the coinbase layout ending the
  input script in the extranonce, distinct channels and prefixes on one
  connection, an invalid user, a zero maximum target, a minimum extranonce
  above the server's, and a target at or below the maximum target
- **OpenStandardMiningChannel:** result fields, and a future first job with
  its merkle root followed by its previous block
- **SubmitShares:** unknown channel and job, a standard share on an extended
  channel, a wrong extranonce size, version bits outside `1fffe000` and an
  invalid timestamp (each dropped), low difficulty, a valid share answered
  once (`duplicate-share`), and a mined block that becomes the top, followed
  by its previous block, a share of the prior job then `stale-share`
- **UpdateChannel and CloseChannel:** unknown channel, zero maximum target,
  a lower maximum target sets the target and sends the job again, and a
  closed channel refuses shares

The share and block tests run where the targets are reachable, as for
stratum v1. A regtest node adds to the stratum v1 configuration above:

```ini
[stratum_v2]
bind = 127.0.0.1:18980
connections = 16
minimum_difficulty = 0.00001  # shares reachable in python
# payout_address = bcrt1q...  # any user identity then opens a channel
# credential = miner:secret   # then --stratum-v2-user=miner.rig1 --stratum-v2-password=secret
```

```bash
pytest test_stratum_v2.py --stratum-v2-port=18980 --native-port=19181 \
  --stratum-v2-user=bcrt1qyg3zyg3zyg3zyg3zyg3zyg3zyg3zyg3zs4w3j0
```

### test_robustness_*.py — Misbehaving clients

These suites act as broken, slow or hostile clients against the
libbitcoin-network HTTP/websocket layer and the JSON-RPC streaming above it.
They do not check answers to valid requests. They check that misbehavior
stays contained.

| File | Layer |
|------|-------|
| `test_robustness_http.py` | HTTP framing and limits, on every HTTP service |
| `test_robustness_ws.py` | RFC 6455 frames, fragmentation, control frames, on every HTTP service |
| `test_robustness_native.py` | native targets, queries, large responses, websocket subscriptions |
| `test_robustness_esplora.py` | Esplora targets, broadcast bodies, large responses, websocket targets |
| `test_robustness_admin.py` | admin log and event subscriptions: hostile targets and filters, unread pushes |
| `test_robustness_electrum.py` | newline-delimited JSON-RPC (Electrum, Sparrow): malformed JSON, envelopes, handshake, volume |
| `test_robustness_bitcoind.py` | bitcoind JSON-RPC over HTTP/TCP, edge parameters, authorization, REST targets |
| `test_robustness_btcd.py` | authorization abuse, JSON-RPC 1.0 over websocket/HTTP, filters, notifications |
| `test_longrun.py` | heavy requests: cancellation, isolation, shutdown (`--bs-pid`, same host) |
| `robustness.py` | raw socket/HTTP/websocket client, canaries, server-side checks |

Each case passes only if the bad connection gets an error or a close within
5 s (unless the input is legitimately incomplete). The autouse `contained`
fixture then checks after every case:

- every service in `--robust-services` still answers a fresh, valid request
  (Electrum and Sparrow over TCP, btcd over websocket, the others over
  HTTP). A service that is not listed is skipped;
- no server-side socket on a service port is left in ESTABLISHED or
  CLOSE_WAIT (released connection slots). This check runs only on the same
  host;
- with `--bs-pid`, the process is alive and no thread stays pegged over two
  3 s windows. `--no-spin-check` turns the CPU check off.

```bash
pytest test_robustness_*.py --btcd-username=robust --btcd-password=robust \
  --bs-pid=$(pidof bs) --robust-connections=32

pytest test_robustness_*.py test_longrun.py \
  --robust-services=native,esplora,admin,electrum,sparrow,bitcoind,btcd \
  --esplora-port=3000 --admin-port=8080 --sparrow-port=50003 \
  --btcd-username=robust --btcd-password=robust \
  --bitcoind-username=user --bitcoind-password=password \
  --bs-pid=$(pidof bs)
```

The full robustness run takes about 1 h 45 min. `test_longrun.py` runs
whole-set scans that occupy the server for minutes.

---

## Electrum protocol versions

The suite adjusts its assertions to the version negotiated by
`server.version`.

| Method / behavior | v1.0 | v1.1 | v1.2 | v1.3 | v1.4 | v1.4.2 | v1.6 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **Server** | | | | | | | |
| `server.version` | ✅ | ✅ | ✅ | ✅ | ✅ ¹ | ✅ ¹ | ✅ ¹ |
| `server.banner` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.features` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.ping` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.add_peer` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.donation_address` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `server.peers.subscribe` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Block header** | | | | | | | |
| `blockchain.block.header` | — ² | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.block.header` with `cp_height` | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| → response keys `header`, `branch`, `root` | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Block headers** | | | | | | | |
| `blockchain.block.headers` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| → response key `hex` (concatenated) | — | — | ✅ | ✅ | ✅ | ✅ | — ³ |
| → response key `headers` (array) | — | — | — | — | — | — | ✅ ³ |
| `blockchain.block.headers` with `cp_height` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Headers subscribe** | | | | | | | |
| `blockchain.headers.subscribe` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| → deserialized `{block_height, version, …}` | ✅ | ✅ | ✅ | — | — | — | — |
| → raw `{height, hex}` | — | — | — | ✅ | ✅ | ✅ | ✅ |
| **Fees** | | | | | | | |
| `blockchain.estimatefee` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.relayfee` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ ⁴ |
| **Script hash** | | | | | | | |
| `blockchain.scripthash.get_balance` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.get_history` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.get_mempool` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.listunspent` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.subscribe` | — ⁵ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.scripthash.unsubscribe` | — | — | — | — | ✅ | ✅ | ✅ |
| **Transactions** | | | | | | | |
| `blockchain.transaction.broadcast` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.get` (raw) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.get` (verbose) | — | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.get_merkle` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `blockchain.transaction.id_from_pos` | — | — | — | — | ✅ | ✅ | ✅ |
| **Pool** | | | | | | | |
| `mempool.get_fee_histogram` | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |

✅ available · — not yet introduced · ✗ removed

¹ Since v1.4, `server.version` must be the first message. The session
fixture sends it before any test.
² v1.0 named it `blockchain.block.get_header`.
³ v1.6 replaced the concatenated `hex` with the `headers` array. The suite
accepts whichever key is present.
⁴ `blockchain.relayfee` was removed in v1.6 (use `server.features`
`fee_rate`). From 1.6 the suite expects any error (`bs` answers
`wrong_version`).
⁵ v1.0 used `blockchain.address.*`.

| Test | Condition | Expected |
|------|-----------|----------|
| `test_blockchain_headers_subscribe` | `< 1.3` | `block_height` and deserialized fields |
| `test_blockchain_headers_subscribe` | `>= 1.3` | `height` and 160-character `hex` |
| `test_blockchain_relayfee` | `< 1.6` | a positive float (BTC/kB) |
| `test_blockchain_relayfee` | `>= 1.6` | an error |
| `test_blockchain_block_headers*` | `< 1.6` | `hex` of `count × 160` characters |
| `test_blockchain_block_headers*` | `>= 1.6` | `headers`, each 160 characters |

### Known xfails

Expected outcomes against `bs`, listed so they are not taken for regressions:

| Test | `bs` answers | Reason |
|------|--------------|--------|
| `test_electrum.py::test_server_add_peer` | `method_not_found` | peer management is not served |
| `test_electrum.py::test_blockchain_block_headers_count_zero` | `bad_request` | `bs` refuses a count of zero, which the protocol does not prohibit |
| `test_electrum.py::test_blockchain_block_headers_cp_target_overflow` | `bad_request` | by design: the test sends a `cp_height` below the target |
| `test_electrum.py::test_blockchain_transaction_broadcast` | `daemon_error` | by design: the test broadcasts a 10 byte payload |
| `test_sparrow.py::test_block_stats_*` (2) | `method_not_found` | `blockchain.block.stats` is a stub |
| `test_btcd_rpc.py::test_authenticate_with_configured_credentials` | `107 unauthorized` | the test repeats `authenticate` on a connection the fixture authenticated, and `bs` refuses a repeated `authenticate` |

With `--bitcoind-auth` and a mainnet node at the top, the functional suites
(all but robustness and stratum) give no other xfail.

---

## Troubleshooting

**Connection refused.** The service is not bound or has `connections = 0`.
Check `bs --settings`.

**401 Unauthorized.** The service has a credential. See
[Credentials](#credentials).

**Timeouts.** Raise `--timeout`. Whole-set scans have their own
`--scan-timeout`.

**Cascading Electrum skips.** The suite's reader survives a timeout, so a
run of skips means the server closed the connection. Check the server log.
An unknown method is answered `-32601 method_not_found` and does not close
the connection.

**Skips against public servers.** Some servers (electrs in particular) do not
answer every method. The test skips.

**`ModuleNotFoundError: No module named 'utils'`.** Run pytest from the
`endpoints/` directory.

### Debug output

| Variable | Test file | Output |
|----------|-----------|--------|
| `ELECTRUM_DEBUG=1` | `test_electrum.py` | each JSON-RPC request and response, with elapsed time |
| `NATIVE_DEBUG=1` | `test_native.py` | each GET and its JSON response, with elapsed time |
| `BITCOIND_DEBUG=1` | `test_bitcoind_rpc.py` | each JSON-RPC request and response, with elapsed time |
| `BTCD_DEBUG=1` | `test_btcd_rpc.py` | each websocket request and response |
| `BTCD_TRIGGER_BLOCK` | `test_btcd_rpc.py` | command run to produce a block for `blockconnected` |
| `ELECTRUM_TRIGGER_HEADERS`, `ELECTRUM_TRIGGER_SCRIPTHASH` | `test_electrum_subscriptions.py` | command run to produce the event |

Use `-s` with a debug variable so pytest does not capture the output.

---

## Writing new tests

### Electrum

Electrum tests share one connection. Use `send_rpc()`: it skips on
connection errors, xfails on server errors, and discards stale responses.

```python
def test_new_electrum_method():
    result = send_rpc("some.method", [param1, param2])
    assert isinstance(result, dict)
    assert "expected_field" in result
```

For a version-dependent assertion, compare `negotiated_version`. For a method
that must be rejected, call `_rpc_raw()` and expect `ValueError`.

### Native REST

```python
def test_new_endpoint(native_config):
    data = get_json(native_config["base_url"], "new/endpoint")
    assert isinstance(data, dict)
    assert "expected_field" in data
```

### Fixtures

| Fixture | Scope | Provides |
|---------|-------|----------|
| `native_config` | session | `host`, `port`, `base_url` |
| `esplora_config` | session | Esplora host, port, base URL |
| `admin_config` | session | admin host and port |
| `bitcoind_rpc_config`, `bitcoind_rest_config` | session | URL, `auth`, timeout |
| `btcd_config` | session | host, port, credential |
| `electrum_config`, `sparrow_config` | session | host, port, protocol range, timeouts |
| `stratum_v1_config` | session | `host`, `port`, `worker`, `password`, `timeout` |
| `robust_env` | session | services, process and limits for the robustness suites |
| `node_log` | session | the `--log` path |

### Guidelines

1. Assert the behavior the specification requires, not only what `bs`
   returns today. A failing test is how a gap is found.
2. Use xfail for server errors that are correct by design, and skip for
   missing prerequisites (a service not bound, a height not reached).
3. Mark tests that need chain data with `requires_height`,
   `requires_current` or `requires_pool`, and tests that wait on real events
   with `slow`.
4. Use the shared data in `utils.ReferenceData`.

## Reference data

`utils.ReferenceData` (mainnet):

| Constant | Value | Description |
|----------|-------|-------------|
| `GENESIS_HEIGHT` | 0 | genesis height |
| `GENESIS_HASH` | `000000000019d668…` | genesis hash |
| `KNOWN_HEIGHT` | 9000 | test block height |
| `KNOWN_BLOCK_HASH` | `00000000415c7a0f…` | block 9000 |
| `KNOWN_TX_HASH` | `a5f71a0b4ccda01f…` | coinbase of block 9000 |
| `FIRST_TX_BLOCK_HEIGHT` | 170 | first block with a transfer |
| `EXAMPLE_SCRIPTHASH` | `6e8c6c7183154cae…` | script hash with confirmed activity |
| `SEGWIT_HEIGHT` | 481824 | first block enforcing SegWit |
| `SEGWIT_BLOCK_HASH` | `0000000000000000001c…` | block 481824 |
| `SEGWIT_TX_HASH_P2WPKH` | `dfcec48bb8491856…` | first native P2WPKH spend |
| `SEGWIT_TX_HASH_P2WPKH2` | `f91d0a8a78462bc5…` | second P2WPKH spend in block 481824 |
| `SEGWIT_TX_HASH_P2WSH` | `461e8a4aa0a0e75c…` | P2WSH output in block 481824 |

## Resources

- [pytest documentation](https://docs.pytest.org/)
- [libbitcoin-server configuration](../docs/configuration.md)
- [bitcoind RPC reference](https://developer.bitcoin.org/reference/rpc/)
- [bitcoind REST interface](https://github.com/bitcoin/bitcoin/blob/master/doc/REST-interface.md)
- [btcd JSON-RPC API](https://github.com/btcsuite/btcd/blob/master/docs/json_rpc_api.md)
- [Esplora HTTP API](https://github.com/Blockstream/esplora/blob/master/API.md)
- [Electrum protocol](https://electrum-protocol.readthedocs.io/)
- [Stratum v1 (Braiins)](https://braiins.com/stratum-v1/docs), [BIP310](https://github.com/bitcoin/bips/blob/master/bip-0310.mediawiki)

## License

These tests are part of libbitcoin-server and are licensed under the GNU
Affero General Public License v3.0.
