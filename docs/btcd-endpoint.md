# btcd-Compatible Endpoint

This document specifies the `btcd` JSON-RPC/WebSocket compatibility endpoint for
libbitcoin-server, modeled on the existing `bitcoind` endpoint (see
`src/protocols/bitcoind/`, added in #795) and on
[btcd's JSON-RPC API](https://btcd.readthedocs.io/en/latest/json_rpc_api.html).

## Background

btcd is a Bitcoin full node written in Go that intentionally separates wallet and
chain services into independent processes: unlike Core/bitcoind, a direct
connection to btcd exposes only chain RPCs, never wallet methods. That matches
libbitcoin-server's own scope (no wallet), which is what makes btcd a good second
compatibility target after bitcoind.

btcd exposes two transports on the same port (default `8334`):

- **HTTP POST**, one JSON-RPC request/response per connection (same shape as the
  existing `bitcoind` endpoint).
- **WebSocket** (`/ws`), btcd's preferred transport: a single connection carries
  many requests plus asynchronous push notifications (new blocks, filtered
  transactions, rescan progress).

Real btcd authenticates via TLS + HTTP Basic Auth, with two credential tiers:
`rpcuser`/`rpcpass` (full access) and `rpclimituser`/`rpclimitpass` (restricted to
a fixed allowlist of chain-read methods). **Decision: this endpoint uses a single
credential tier** (reusing `network::settings::http_server`'s existing
`username`/`password`/`authenticated()`, same as `bitcoind.*`). Sensitive methods
(`stop`, mining/peer-management) are stubbed to `not_implemented` unconditionally,
so a limited-user tier adds no security value here; it can be revisited if a
future phase implements a real `stop` path.

## Architecture

```
network::protocol_http
└── server::protocol_bitcoind_rpc
    └── server::protocol_btcd_rpc         (new)

network::channel_http
└── server::channel_http
    └── server::channel_btcd              (new)

server::session_btcd = session_server<protocol_btcd_rpc>   (new)
```

`protocol_btcd_rpc` inherits `protocol_bitcoind_rpc`, which already implements the
chain-read/rawtx handlers both APIs share (`getbestblockhash`, `getblock`,
`getblockhash`, `getblockheader`, `getblockcount`, `gettxout`, `getrawtransaction`,
`sendrawtransaction`, ...). It adds a second dispatcher for `interface::btcd`
(btcd-only methods) and overrides `network::protocol_http::dispatch_websocket()` —
the extension point `protocol_http` already exposes for handling an HTTP→WS
upgrade — to route btcd's WS-only notification/subscription methods.

`channel_btcd` extends `channel_http` with an `authenticated_` flag, set once the
WS `authenticate` command (or HTTP Basic Auth header) succeeds.

Notification/subscription state (watched blocks, watched scripts/outpoints)
follows the pattern already established in `protocol_electrum`
(`src/protocols/electrum/protocol_electrum.cpp`): a small map guarded by a
dedicated notification strand, populated by `node::chase` event callbacks, fed to
clients via `do_<event>`/`complete_*` handlers. Model the btcd notifier on that
shape rather than inventing a new one.

New files:

- `include/bitcoin/server/channels/channel_btcd.hpp`
- `include/bitcoin/server/protocols/protocol_btcd_rpc.hpp`
- `include/bitcoin/server/interfaces/btcd.hpp` (method table, see
  `interfaces/bitcoind_rpc.hpp` for the pattern)
- `src/protocols/btcd/protocol_btcd_rpc.cpp`
- `src/protocols/btcd/protocol_btcd_rpc_subscribe.cpp`
- `test/protocols/btcd/btcd_rpc.cpp`

Modified: `channels.hpp`, `protocols.hpp`, `sessions.hpp`, `settings.hpp`,
`server_node.hpp`/`.cpp`, `parser.cpp` (all following the `bitcoind` wiring as a
template).

## Method Scope

### Already covered (inherited from `protocol_bitcoind_rpc`)

`getbestblockhash`, `getblock`, `getblockhash`, `getblockheader`, `getblockcount`,
`gettxout`, `getrawtransaction`, `sendrawtransaction`. No btcd-specific work
needed; response shapes already match Core/btcd conventions closely enough for
Phase A. **Caveat verified during Phase A implementation**: these are
currently reachable only via plain http post to the same endpoint (unchanged
from `bitcoind`), not yet over the ws connection itself. `dispatch_websocket`
only routes to the btcd-only extension dispatcher; bridging the inherited
handlers' post-oriented private send path (which caches the original
`http::request` for header derivation, unavailable for a ws frame) into a
ws-reachable form is real work, scheduled for Phase B. Until then, a
ws-connected client (required for `authenticate`/`session`/`notifyblocks`)
cannot also call `getblockcount` etc. on that same connection — a real
lnd/btcwallet client would need this bridged to be useful in practice.

### Phase A — wiring

`channel_btcd`, `protocol_btcd_rpc`, `authenticate`, `session` (must be
functionally correct, not stubbed — see [lnd Compatibility](#lnd-compatibility)),
`notifyblocks`/`stopnotifyblocks`, `not_implemented` stubs for every other btcd
method, Boost.Test suite.

### Phase B — lnd-critical, promoted ahead of generic filler

`getcurrentnet` (network-match check lnd/btcwallet performs once at connect),
`loadtxfilter`, `rescanblocks` (guard behind `address_enabled`, same guard used
by the address-indexing-dependent electrum methods). None of these are on the
critical path for a generic btcd client — they're promoted here specifically
because `btcwallet`'s `chain.RPCClient` and lnd's own `chainntnfs/btcdnotify`
depend on them (see [lnd Compatibility](#lnd-compatibility)).

### Phase C — remaining plain RPC methods (generic btcd-tooling compatibility)

`getdifficulty`, `getinfo`, `getnettotals`, `getnetworkhashps`,
`createrawtransaction`, `decoderawtransaction`, `decodescript`,
`validateaddress`, `help`. No specific named consumer — these round out
compatibility for generic btcd-speaking tooling (block explorers, mining-pool
stats, monitoring dashboards, tx-building/debug scripts), not lnd.

### Permanently stubbed (`not_implemented`, all phases)

- **`stop`** — no secure remote-shutdown path exists in libbitcoin-server; always
  returns `not_implemented` regardless of auth tier or config.
- **`notifynewtransactions`**, and the deprecated WS methods `notifyreceived`,
  `stopnotifyreceived`, `notifyspent`, `stopnotifyspent`, `rescan` (all superseded
  upstream by `loadtxfilter`/`rescanblocks`) — no mempool exists in v4, revisit in
  v5.
- **`submitblock`** — future-phase only, not in current scope.
- Mining (`getgenerate`, `gethashespersec`, `getmininginfo`, `setgenerate`) and
  peer-management (`addnode`, `getaddednodeinfo`, `getconnectioncount`,
  `getpeerinfo`) — out of scope; libbitcoin-server is not a miner and peer
  management is a node-config concern, not an RPC surface here.

## lnd Compatibility

btcd's own websocket API is one of lnd's three supported chain backends (the
others being `bitcoind` via ZMQ, and `neutrino`). Hooking an `lnd` node up to
libbitcoin-server through this endpoint means satisfying the calls
`btcwallet`'s `chain.RPCClient` (`btcsuite/btcwallet/chain/btcd.go`) and lnd's own
`chainntnfs/btcdnotify` package make against a btcd-style server. Verified against
both sources:

**Plain RPC** (all either inherited from bitcoind or trivial to add):
`getbestblockhash`/`getblockcount` (chain tip), `getblockhash`, `getblockheader`
(chain-view / `IsCurrent` checks), `getblock` (verbose, block scanning),
`getrawtransaction`, `sendrawtransaction` (broadcasting funding/commitment/
sweep/justice transactions), `gettxout` (UTXO-existence checks), `getcurrentnet`
(network-match validation performed once at connect — scheduled in Phase B).

**WebSocket notification surface — the real gap.** Both `btcwallet`'s RPCClient
and lnd's `btcdnotify` depend on:

- `NotifyBlocks` → `OnBlockConnected`/`OnBlockDisconnected` — covered by Phase A.
- A working `session` on connect — `btcwallet` uses the session id to detect a
  reconnect to a fresh server instance, which must trigger re-registration of all
  filters/rescans. If this is left as a stub, reconnect handling is undefined; it
  should be implemented for real in Phase A, not deferred.
- **`loadtxfilter`** (address/outpoint watch-list registration) plus
  block-filtered notification delivery (`OnFilteredBlockConnected`/
  `OnRedeemingTx`/`OnRecvTx` equivalents) — this is the mechanism both the wallet
  and lnd's own notifier use to detect channel-funding confirmations and
  on-chain spends of channel outputs (breach/force-close detection). It is not
  optional legacy behavior; modern `btcwallet`/`lnd` don't fall back to the
  deprecated `notifyreceived`/`notifyspent` path. This is why it's scheduled in
  Phase B, ahead of Phase C's generic filler methods — it's more load-bearing
  for lnd than any Phase C method.
- **`rescan`/`rescanblocks`** — used at wallet startup (birthday scan) and after
  any downtime to replay historical blocks against the loaded filter. Also
  scheduled in Phase B for the same reason.

**Hard blocker: mempool.** Both sources confirm unconfirmed-transaction awareness
is an exercised, not merely optional, code path: `OnRecvTx`/`OnRedeemingTx` fire
for mempool-resident transactions (distinguished by a nil `BlockDetails`), and
lnd's notifier has a dedicated mempool-spend lookup path
(`LookupInputMempoolSpend`) used for fast breach/force-close reaction ahead of
confirmation. **libbitcoin-server v4 has no mempool**, so this notification class
cannot be implemented now — confirmed-only fallback is straightforward (fire the
same callbacks only on block connection), but it means lnd would lose
unconfirmed-spend awareness and any zero-conf-style UX. Final safety in Lightning
still rests on confirmation + CSV/CLTV timeout margins, so this is a
responsiveness/UX degradation rather than a correctness break, but it's a real,
user-visible limitation worth stating plainly rather than glossing over. Revisit
once a v5 mempool exists.

**Not needed for lnd**: fee estimation — lnd's default fee estimator for the
`btcd` backend is a web API (`chainfee.WebAPIEstimator`), not an RPC call to the
node, so `estimatesmartfee`-equivalent support is not on the critical path.

## Configuration

New `[btcd]` section, mirroring `[bitcoind]`'s shape in `parser.cpp`/
`settings.hpp` (`bind`, `safe`, `cert_auth`, `cert_path`, `key_path`, `key_pass`,
`username`, `password`, `connections`, `inactivity_minutes`,
`expiration_minutes`, `minimum_buffer`, `maximum_request`, `host`, `origin`,
`allow_opaque_origin`). No dedicated `stop`-gating config key: `stop` has no
conditional path at all right now (always `not_implemented`, unconditionally),
so a flag controlling it would be dead configuration. If a real guarded `stop`
path is ever implemented, add the gating key then.

Default port: `8334` (matches upstream btcd).

## Code Conventions (apply from the outset, per #795 review feedback)

The `bitcoind` PR (#795) went through several review rounds with @evoskuil before
merging; apply these proactively on the btcd PR to avoid repeating the same
cycle:

- No inline (header-implemented) non-template method bodies — put them in
  `.cpp` files; keeps build/rebuild times down for a developer-facing lib.
- Protocol-specific json/serialization helpers are protected static methods on
  the base protocol class (inherited by subclasses), not free functions in the
  `server` namespace. Split into a `_json.cpp` for organization if it grows.
- Pass `hash_cptr` and other small shared_ptr objects as `const&`, never by
  value — a copy needlessly bumps the refcount and hits a shared lock.
- Prefer `to_shared(std::move(x))` over manual move-then-construct.
- Use `std::from_chars` (C++17) directly; add an explicit leading-zero guard if
  canonical parsing matters (`from_chars` alone accepts `"01"`).
- Arithmetic: `add1()`, `floored_subtract()` — never raw `-`, the store is
  concurrent and `top >= height` is not assured — `possible_sign_cast<>`,
  `possible_wide_cast<>`, `is_lesser<size_t>()` over raw casts,
  `ceilinged_add()`/`is_overflow()` for additions that could overflow,
  `std::next()` instead of pointer arithmetic.
- Cache constants that are invariant for the process lifetime (e.g. genesis
  hash lookups) as function-local `static`; use `constexpr base16_hash("...")`
  instead of runtime `encode_hash`/string comparisons in hot paths.
- Use the query layer's existing fluent methods instead of re-deriving values:
  `get_confirmed_headers` + `get_wire_header` instead of hand-looping
  `chain::header` objects, `get_tx_height(height, link)` alone instead of extra
  containment checks, the stored context field (single `get_context` query) for
  median-time-past instead of recomputing over 11 headers.
- Stream into one pre-sized buffer (`stream::out::fast` + a typed writer)
  instead of looping and copying per item. Don't mask a per-item store failure
  inside such a loop as "not found" — that's `database::error::integrity`
  (`send_internal_server_error`), a different failure class.
- Naming: `send_text()`/`send_json()`/`to_data()`/`to_text()` — "data" means
  byte array, "text" means base16 string; avoid "hex"/"bin" abbreviations.
- Pure type→json serialization belongs in **libbitcoin-system** as a
  `chain/json/<type>` serializer (see `system/chain/json/block.hpp`+`.cpp`+
  test), not hand-rolled long-term in the protocol layer. File a system-repo PR
  if a new btcd-only json shape is needed, rather than keeping it local.
- Tests: Boost.Test only for Unit/Component/Functional tiers, no external
  dependencies (no Python) — that's `test/protocols/btcd/`. Data-driven loops
  only over genuine external vectors; no line-wrapping; precompute `constexpr`
  hashes/expectations at file scope. The Python suite under `endpoints/` is a
  separate, optional **acceptance** tier (run against a live compiled node),
  not a merge requirement.
