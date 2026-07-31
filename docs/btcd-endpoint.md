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
a fixed allowlist of chain-read methods). **Decision: this endpoint does not build
a bespoke two-tier scheme.** It reuses `network::settings::http_server`'s
credential model as-is (`btcd.credential = username:password[:method,...]`, same
config shape as `bitcoind.*`), which happens to already support scoping a
credential to a method allowlist generically; `channel_btcd::permitted()` enforces
that scope over both http and ws. Sensitive methods (`stop`, mining/peer
management) are stubbed to `not_implemented` unconditionally regardless of scope,
so this is revisited only if a real `stop` path is implemented later.

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

**A `session_handshake<protocol_btcd_auth, protocol_btcd_rpc>` variant of this was
tried and reverted.** The idea was to model btcd's auth on
`protocol_electrum_version`'s handshake pattern: a dedicated protocol object that
gates attachment of the real one. It doesn't fit. `session_handshake` calls
`channel->resume()` immediately after the handshake protocol attaches, on the
assumption that the handshake can only complete by reading a real message
(true for electrum's `server.version`, true for p2p's version message) — but
btcd's handshake, when no credential is configured, completes without reading
anything at all. That created a real async gap between "handshake decided
it's done" and "`protocol_btcd_rpc` has actually subscribed its handlers" —
harmless for a websocket client (the upgrade round-trip masks it), but a
plain HTTP client can land a request in that gap before anything is
listening, and did, reliably, in testing. Real btcd has no such gap because
it has no handshake protocol at all: `session_server` (used here) never calls
`resume()` until *after* `attach_protocols()` has synchronously subscribed
the real handlers — the same shape bitcoind already uses, and the reason
bitcoind never had this problem. `authenticate` is just an ordinary
extension method on `protocol_btcd_rpc`, exactly matching real btcd, where it
is described as "disallowed when basic auth has already been established" —
an alternative to Basic Auth for the one transport that can't carry headers
per-message, not a mandatory step every connection passes through.

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

### Inherited from `protocol_bitcoind_rpc`

`getbestblockhash`, `getblock`, `getblockhash`, `getblockheader`, `getblockcount`,
`gettxout`, `getrawtransaction`, `sendrawtransaction`. No btcd-specific work
needed; response shapes already match Core/btcd conventions closely enough as-is.
**Bridged into the ws dispatcher**: `dispatch_websocket` tries
the btcd-only extension dispatcher first, then falls back to
`protocol_bitcoind_rpc::dispatch_rpc` when that dispatcher reports
`unexpected_method` — a pure method-name lookup miss, never conflated with an
argument-mismatch error, so the fallback cannot mask a real handler fault.
`send_rpc` (the shared sender all inherited chain handlers funnel through)
branches on `websocket()`: the post path is unchanged (still derives headers
from the cached `http::request`), while the ws path sends a minimal response
with no header enrichment — ws frames are synthesized with no real headers to
echo in the first place (`channel_http::create_request()`), matching
`protocol_btcd_rpc`'s own ws senders. A ws-connected client (required for
`authenticate`/`session`/`notifyblocks`) can now also call `getblockcount`
etc. on that same connection, as a real lnd/btcwallet client needs.

**`getblockchaininfo`** (shared with `bitcoind`, not btcd-only) also reports a
`bip9_softforks.taproot` entry unconditionally — real btcd includes this too
(taproot is a permanent, long-since-locked-in consensus rule, no live
signaling left to track). Added specifically because lnd's
`chainreg.backendSupportsTaproot` requires this key's presence before it will
treat *any* btcd/bitcoind backend as usable.

### btcd-only methods — implemented

- **`authenticate`**, **`session`** — ordinary extension methods, not a
  handshake stage (see [Architecture](#architecture)); `session` returns a
  real per-channel id, `btcwallet` uses it to detect reconnect-to-a-fresh-
  server and re-arm filters.
- **`notifyblocks`/`stopnotifyblocks`** — subscribes to
  `node::chase::organized`/`reorganized`, pushes `blockconnected`/
  `blockdisconnected` (positional `[hash, height, time]`, verified against
  `btcsuite/btcd/btcjson/chainsvrwsntfns.go`).
- **`getcurrentnet`** — the network-match check lnd/btcwallet performs once at
  connect; returns `network_settings().identifier`, the same p2p handshake
  magic real btcd returns.
- **`loadtxfilter`** + filtered block notifications
  (`filteredblockconnected`/`filteredblockdisconnected`) + **`rescanblocks`** —
  a per-connection in-memory watch-list (base58 p2kh/p2sh via
  `wallet::payment_address`, bech32/bech32m p2wpkh/p2wsh/p2tr via
  `wallet::witness_address` + `chain::script::output_pattern()`, plus
  outpoints), matched by direct script inspection of each newly-connected (or,
  for `rescanblocks`, explicitly named historical) block's own transactions —
  not the persisted address index (`address_enabled`/`get_history`), which is
  the wrong tool for "does this one block match this small watch-list" and
  would needlessly require the address index to be built at all. An output
  that matches a watched address has its own outpoint auto-added to the
  watched set (matching real btcd's `wsClientFilter`), so a later spend of it
  also matches without an explicit re-subscribe. This is the mechanism both
  `btcwallet`'s `chain.RPCClient` and lnd's own `chainntnfs/btcdnotify`
  depend on (see [lnd Compatibility](#lnd-compatibility)).
- **`getbestblock`** — a distinct btcd extension from `getbestblockhash`,
  returning `{hash, height}` of the chain tip together. Found to be required
  by `btcwallet`'s own separate rpcclient connection (not lnd's own
  `chain.RPCClient`, which uses `getbestblockhash`) during wallet chain-sync
  bootstrap, via a real lnd integration test.
- **`rescan`** (deprecated upstream, superseded by `rescanblocks`) — implemented
  only for its empty-addresses/empty-outpoints case: real btcd's own
  `handleRescan` skips scanning entirely and reports immediate completion
  (`rescanfinished` notification carrying the current chain tip) when given
  nothing to watch. This is the exact call `btcwallet`'s own rpcclient makes
  purely to bootstrap its initial sync starting point — found via the same
  real lnd integration test. A non-empty address/outpoint list remains
  `not_implemented` (see [Permanently stubbed](#permanently-stubbed-not_implemented)).
- Every btcd-only method above is also reachable over plain HTTP POST, not
  only the websocket connection — real lnd/btcwallet clients issue
  capability-check calls (e.g. `getinfo`) this way immediately on connect,
  before establishing the persistent ws connection that subscriptions need.
  Found missing (a real, silent connection-killing bug) via a live lnd
  integration test; fixed by adding `protocol_bitcoind_rpc::
  dispatch_extension`, the post-side mirror of the ws-side fallback described
  above.
- **`getdifficulty`, `getinfo`, `getnettotals`, `getnetworkhashps`,
  `createrawtransaction`, `decoderawtransaction`, `decodescript`,
  `validateaddress`, `help`** — no specific named consumer; these round out
  compatibility for generic btcd-speaking tooling (block explorers,
  mining-pool stats, monitoring dashboards, tx-building/debug scripts), not
  lnd.

Notes on scope/fidelity (all verified against real btcd's `btcjson`
result/command structs before implementing):

- `createrawtransaction`/`decoderawtransaction`/`decodescript` reuse existing
  serializers rather than re-deriving them: `bitcoind(tx)`
  (`system::chain::json::transaction`) already produces the bare
  decoderawtransaction shape (`inject_tx_context` — block-context fields —
  is a separate, additive call, skipped here since a standalone hex tx has no
  block context); `chain::script::to_string()` already produces the "asm"
  disassembly. There is no existing script-type classifier
  (pubkeyhash/scripthash/multisig/etc.) or reverse address encoder anywhere
  in the codebase, so `decodescript`'s `type`/`address`/`p2sh` fields and
  `validateaddress` are new, built from `chain::script::output_pattern()` and
  `wallet::payment_address`/`witness_address` (the same dual-parse approach
  as `loadtxfilter`'s, kept as a separate small helper rather than forcing
  a shared abstraction onto that already-tested code).
- `getnetworkhashps` approximates from the requested height's difficulty and
  a fixed 600s target spacing, not a true windowed average over the `blocks`
  parameter — there is no existing big-integer cumulative-work-over-a-range
  helper to build a real windowed estimate from, and this method has no named
  consumer, so the simpler approximation was chosen over adding one.
- `getnettotals`'s byte counters are not tracked by this node and are
  reported as zero, matching the same honesty convention the inherited
  bitcoind `getnetworkinfo` handler already uses for untracked
  peer-dependent fields (rather than `not_implemented`).
- `help` lists method names only; no per-command argument usage text.

### Permanently stubbed (`not_implemented`)

- **`stop`** — no secure remote-shutdown path exists in libbitcoin-server; always
  returns `not_implemented` regardless of auth tier or config.
- **`notifynewtransactions`**, and the deprecated WS methods `notifyreceived`,
  `stopnotifyreceived`, `notifyspent`, `stopnotifyspent` (all superseded
  upstream by `loadtxfilter`/`rescanblocks`) — no mempool exists in v4, revisit in
  v5.
- **`rescan`'s non-empty-address/outpoint case** — a real historical block scan
  (chunked `rescanprogress` notifications, per-tx `recvtx`/`redeemingtx`
  notifications — a different wire shape than `filteredblockconnected`'s
  per-block shape —, reorg recovery) that no observed real caller needs;
  `loadtxfilter`/`rescanblocks` already cover actual address/outpoint watching
  for lnd. The empty-case bootstrap call is implemented (see above).
- **`submitblock`** — not in current scope.
- Mining (`getgenerate`, `gethashespersec`, `getmininginfo`, `setgenerate`) and
  peer-management (`addnode`, `getaddednodeinfo`, `getconnectioncount`,
  `getpeerinfo`) — out of scope; libbitcoin-server is not a miner and peer
  management is a node-config concern, not an RPC surface here.

## lnd Compatibility

btcd's own websocket API is one of lnd's three supported chain backends (the
others being `bitcoind` via ZMQ, and `neutrino`). Hooking an `lnd` node up to
libbitcoin-server through this endpoint means satisfying the calls
`btcwallet`'s `chain.RPCClient` (`btcsuite/btcwallet/chain/btcd.go`) and lnd's own
`chainntnfs/btcdnotify` package make against a btcd-style server.

**Verified end-to-end against a real lnd 0.21.1-beta binary**: pointed
directly at a libbitcoin-server instance running this endpoint
(`--bitcoin.node=btcd`), lnd reaches `Chain backend is fully synced!` —
wallet opened/unlocked, chain-sync bootstrap completed, headers walked to
the live chain tip. Every gap below was found this way, not by reading
source alone; each is confirmed fixed by a subsequent clean run one step
further than the last.

**Plain RPC** (all either inherited from bitcoind or btcd extensions):
`getbestblockhash`/`getblockcount` (chain tip), `getblockhash`, `getblockheader`
(chain-view / `IsCurrent` checks), `getblock` (verbose, block scanning),
`getrawtransaction`, `sendrawtransaction` (broadcasting funding/commitment/
sweep/justice transactions), `gettxout` (UTXO-existence checks), `getcurrentnet`
(network-match validation performed once at connect), `getbestblock`
(`btcwallet`'s own separate rpcclient connection, used during its own
chain-sync bootstrap — distinct from `getbestblockhash`).

**WebSocket notification surface.** Both `btcwallet`'s RPCClient
and lnd's `btcdnotify` depend on:

- `NotifyBlocks` → `OnBlockConnected`/`OnBlockDisconnected` — implemented.
- A working `session` on connect — `btcwallet` uses the session id to detect a
  reconnect to a fresh server instance, which must trigger re-registration of all
  filters/rescans. Implemented for real (not stubbed), for exactly this reason.
- **`loadtxfilter`** (address/outpoint watch-list registration) plus
  block-filtered notification delivery (the modern wire methods
  `filteredblockconnected`/`filteredblockdisconnected`, which drive lnd's
  `OnFilteredBlockConnected` client callback — verified against
  `btcsuite/btcd/btcjson/chainsvrwsntfns.go` and `rpcclient/notify.go`; the
  deprecated per-tx `OnRecvTx`/`OnRedeemingTx` callbacks are fed by a
  different, older wire pair (`recvtx`/`redeemingtx`) that modern
  `btcwallet`/`lnd` don't use) — implemented. This is the mechanism
  both the wallet and lnd's own notifier use to detect channel-funding
  confirmations and on-chain spends of channel outputs (breach/force-close
  detection); not optional legacy behavior.
- **`rescanblocks`** — used at wallet startup (birthday scan) and after any
  downtime to replay historical blocks against the loaded filter —
  implemented, same match logic as live filtered notifications,
  applied to explicitly named blocks instead of the newly-connected one.
- **`rescan`** (older, deprecated upstream) — `btcwallet`'s own rpcclient
  still calls this once, with an empty address/outpoint list, purely to
  bootstrap its initial sync starting point; implemented for that case (see
  [Method Scope](#method-scope)). A general historical address/outpoint scan
  remains stubbed; no observed real caller needs it.

**Hard blocker: mempool.** Both sources confirm unconfirmed-transaction awareness
is an exercised, not merely optional, code path: `relevanttxaccepted` (the modern
wire method, driving lnd's `OnRelevantTxAccepted` callback; armed by
`notifynewtransactions`, already permanently stubbed — see above) fires for a
mempool-resident transaction matching the loaded filter, and
lnd's notifier has a dedicated mempool-spend lookup path
(`LookupInputMempoolSpend`) used for fast breach/force-close reaction ahead of
confirmation. **libbitcoin-server v4 has no mempool**, so `relevanttxaccepted`
cannot be implemented now — the confirmed-only fallback (`filteredblockconnected`)
covers the same address/outpoint watch-list, just one block
later than a mempool-aware node would, so it means lnd would lose
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
`credential`, `connections`, `inactivity_minutes`,
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
