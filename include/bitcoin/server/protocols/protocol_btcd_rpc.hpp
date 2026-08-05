/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_RPC_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_RPC_HPP

#include <atomic>
#include <memory>
#include <set>
#include <unordered_set>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

namespace libbitcoin {
namespace server {

/// btcd-compatible json-rpc-v1 over websocket. Inherits the standard chain
/// handlers (getblockcount, getblock, etc.) from protocol_bitcoind_rpc, which
/// remain reachable both via the inherited http post path (unchanged) and,
/// bridged, over this class's ws dispatcher (dispatch_websocket falls back to
/// protocol_bitcoind_rpc::dispatch_rpc when the btcd-only dispatcher reports
/// unexpected_method). Adds the btcd-only extension methods (session/
/// notification/filter/admin) over websocket via a second dispatcher
/// (dispatch_websocket, below).
///
/// Attaches immediately on connect (session_btcd = session_server<
/// protocol_btcd_rpc>, no separate handshake protocol/stage) -- matching real
/// btcd's own model, which has no handshake either: HTTP Basic Auth is
/// checked synchronously per plain-http request (channel_http::authorized(),
/// unchanged from bitcoind, and re-checked on every request since ordinary
/// HTTP headers accompany each one). Over the persistent ws connection,
/// nothing is re-sent or re-checked per-message -- ws data frames
/// structurally cannot carry a per-message Authorization header at all, so
/// there is no way to compose one even if this wanted to. Instead,
/// authorization for the whole connection is established exactly once, by
/// whichever of two events happens first: real Basic Auth presented on the
/// one-time ws upgrade request (which does have real headers, latched via
/// the same channel_http mechanism as a plain http request), or the in-band
/// 'authenticate' extension method below, called once as a substitute for
/// that header. Either way the result (a credential digest) is cached on
/// channel_btcd for the connection's lifetime (see channel_btcd::
/// set_authenticated/permitted) and consulted, not re-verified, on every
/// subsequent call. See docs/btcd-endpoint.md
/// for why an earlier session_handshake<protocol_btcd_auth, protocol_btcd_rpc>
/// design (modeled on electrum/p2p's version negotiation) was reverted: those
/// protocols' handshakes always wait for a real message to complete, but
/// btcd's has no credential-required case to wait for -- modeling it on that
/// pattern introduced an async attach gap real btcd never has, which a plain
/// http client (no upgrade round-trip to mask the gap, unlike ws) could land
/// in before this class ever subscribed its handlers.
///
class BCS_API protocol_btcd_rpc
  : public server::protocol_bitcoind_rpc,
    protected network::tracker<protocol_btcd_rpc>
{
public:
    // Replace channel_t so session_server creates channel_btcd instances.
    using channel_t = channel_btcd;

    typedef std::shared_ptr<protocol_btcd_rpc> ptr;
    using btcd_interface = interface::btcd;
    using btcd_dispatcher = network::rpc::dispatcher<btcd_interface>;

    inline protocol_btcd_rpc(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_bitcoind_rpc(session, channel, options),
        network::tracker<protocol_btcd_rpc>(session->log),
        options_(options),
        btcd_channel_(std::dynamic_pointer_cast<channel_btcd>(channel)),
        p2kh_(session->system_settings().forks.difficult ?
            system::wallet::payment_address::mainnet_p2kh :
            system::wallet::payment_address::testnet_p2kh),
        p2sh_(session->system_settings().forks.difficult ?
            system::wallet::payment_address::mainnet_p2sh :
            system::wallet::payment_address::testnet_p2sh)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Dispatch btcd ws frames: tries the btcd-only dispatcher first, then
    /// falls back to the inherited chain dispatcher (dispatch_rpc) so
    /// standard methods (getblockcount etc.) are also reachable over ws, not
    /// only via the inherited handle_receive_post's http post path.
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Post-side mirror of the above: lets handle_receive_post reach
    /// btcd-only methods (e.g. getinfo) over plain http post too. Real
    /// clients are not guaranteed to call these exclusively over an
    /// upgraded ws connection -- confirmed via a live lnd integration test,
    /// whose chain.RPCClient issues getblockchaininfo/getinfo as post-based
    /// capability checks immediately on connect, before any subscription
    /// traffic that actually requires ws (notifyblocks/loadtxfilter/etc).
    code dispatch_extension(
        const network::rpc::request_t& message) NOEXCEPT override;

    /// Handlers (session/handshake). 'authenticate' is a real, ordinary
    /// extension method here (not a separate handshake stage, see class
    /// comment): it hashes the given credential the same way
    /// config::credential does, checks it via options_.authorized(), and on
    /// success latches the digest onto channel_btcd so permitted()'s method
    /// scoping can apply to subsequent ws calls. interface::btcd's method
    /// table has no notion of "not yet authenticated", so this always runs
    /// regardless of current state -- matches real btcd's own tolerance of a
    /// repeat authenticate call (only disallowed once basic auth has already
    /// been established on the ws upgrade, not enforced here since that
    /// case is harmless to allow). A failed attempt ends the session
    /// (dispatch_websocket sends the error before stopping), matching real
    /// btcd's documented behavior of closing the connection on invalid
    /// credentials rather than leaving it open for a retry.
    bool handle_authenticate(const code& ec, btcd_interface::authenticate,
        const std::string& username, const std::string& password) NOEXCEPT;
    bool handle_session(const code& ec, btcd_interface::session) NOEXCEPT;

    /// Handler (network magic; btcwallet/lnd check this once at connect to
    /// confirm they're talking to the expected network).
    bool handle_get_current_net(const code& ec,
        btcd_interface::get_current_net) NOEXCEPT;

    /// Handler (chain tip; btcwallet's own rpcclient connection calls this
    /// during wallet chain-sync bootstrap, distinct from getbestblockhash
    /// which lnd's own chain.RPCClient uses).
    bool handle_get_best_block(const code& ec,
        btcd_interface::get_best_block) NOEXCEPT;

    /// Handlers (generic btcd-tooling compatibility, no lnd consumer -- see
    /// docs/btcd-endpoint.md).
    bool handle_get_difficulty(const code& ec,
        btcd_interface::get_difficulty) NOEXCEPT;
    bool handle_get_info(const code& ec, btcd_interface::get_info) NOEXCEPT;
    bool handle_get_net_totals(const code& ec,
        btcd_interface::get_net_totals) NOEXCEPT;
    bool handle_get_network_hash_ps(const code& ec,
        btcd_interface::get_network_hash_ps, uint32_t blocks,
        int32_t height) NOEXCEPT;
    bool handle_create_raw_transaction(const code& ec,
        btcd_interface::create_raw_transaction,
        const network::rpc::array_t& inputs,
        const network::rpc::object_t& outputs, uint32_t locktime) NOEXCEPT;
    bool handle_decode_raw_transaction(const code& ec,
        btcd_interface::decode_raw_transaction,
        const std::string& hexstring) NOEXCEPT;
    bool handle_decode_script(const code& ec,
        btcd_interface::decode_script, const std::string& hex) NOEXCEPT;
    bool handle_validate_address(const code& ec,
        btcd_interface::validate_address,
        const std::string& address) NOEXCEPT;
    bool handle_help(const code& ec, btcd_interface::help,
        const std::string& command) NOEXCEPT;

    /// Address string (base58 p2kh/p2sh or bech32/bech32m p2wpkh/p2wsh/p2tr)
    /// to output script, for createrawtransaction. A separate, standalone
    /// helper from parse_filter_addresses (which classifies into this
    /// class's watch-list hash buckets, not a reusable script) -- kept
    /// duplicated rather than forcing a shared abstraction onto already-
    /// tested filter-parsing code for a small amount of address-string
    /// parsing logic.
    code parse_output_script(const std::string& text,
        system::chain::script& out) NOEXCEPT;

    /// Handlers (block subscription).
    bool handle_notify_blocks(const code& ec,
        btcd_interface::notify_blocks) NOEXCEPT;
    bool handle_stop_notify_blocks(const code& ec,
        btcd_interface::stop_notify_blocks) NOEXCEPT;

    /// Handlers (mempool subscription, not_implemented pending v5 mempool).
    bool handle_notify_new_transactions(const code& ec,
        btcd_interface::notify_new_transactions, bool verbose) NOEXCEPT;
    bool handle_stop_notify_new_transactions(const code& ec,
        btcd_interface::stop_notify_new_transactions) NOEXCEPT;

    /// Handlers (address/outpoint filtering). loadtxfilter never itself
    /// triggers notifications (matching real btcd) -- notifyblocks remains
    /// the only thing that arms delivery; once armed, do_block_connected/
    /// do_block_disconnected send filteredblockconnected/disconnected
    /// alongside the existing unfiltered blockconnected/disconnected,
    /// unconditionally (an empty/never-loaded filter just yields an empty
    /// subscribedtxs array, matching real btcd's own behavior). rescanblocks
    /// replays the same match logic against explicitly named historical
    /// blocks using the already-loaded filter.
    bool handle_load_tx_filter(const code& ec,
        btcd_interface::load_tx_filter, bool reload,
        const network::rpc::value_t& addresses,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_rescan_blocks(const code& ec,
        btcd_interface::rescan_blocks,
        const network::rpc::value_t& blockhashes) NOEXCEPT;

    /// Filter parsing (loadtxfilter). Merges into the existing filter unless
    /// reload clears it first. Returns error::invalid_argument if any address
    /// fails to parse as either a base58 (p2kh/p2sh) or bech32/bech32m
    /// (p2wpkh/p2wsh/p2tr) address, or any outpoint is malformed.
    code parse_filter_addresses(bool reload,
        const network::rpc::value_t& addresses) NOEXCEPT;
    code parse_filter_outpoints(bool reload,
        const network::rpc::value_t& outpoints) NOEXCEPT;

    /// Match a block's transactions against the loaded filter: an output
    /// paying a watched address, or an input spending a watched outpoint.
    /// A matched address-output's own outpoint is added to the watched set
    /// (auto-tracking a future spend of it), matching real btcd's
    /// wsClientFilter behavior. Returns the hex-encoded (witness) matched
    /// transactions, in block order; empty if none matched.
    network::rpc::array_t match_filtered_transactions(
        const system::chain::block& block) NOEXCEPT;

    /// Handler (admin, permanently not_implemented).
    bool handle_stop(const code& ec, btcd_interface::stop) NOEXCEPT;

    /// Handlers (deprecated, permanently not_implemented).
    bool handle_notify_received(const code& ec,
        btcd_interface::notify_received,
        const network::rpc::value_t& addresses) NOEXCEPT;
    bool handle_stop_notify_received(const code& ec,
        btcd_interface::stop_notify_received,
        const network::rpc::value_t& addresses) NOEXCEPT;
    bool handle_notify_spent(const code& ec,
        btcd_interface::notify_spent,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_stop_notify_spent(const code& ec,
        btcd_interface::stop_notify_spent,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_rescan(const code& ec, btcd_interface::rescan,
        const std::string& beginblock, const network::rpc::value_t& addresses,
        const network::rpc::value_t& outpoints,
        const std::string& endblock) NOEXCEPT;

    /// Chase event subscription (block connect/disconnect for notifyblocks).
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;
    void do_block_connected(node::header_t link) NOEXCEPT;
    void do_block_disconnected(node::header_t link) NOEXCEPT;

    /// Senders (btcd ws envelope, distinct id/version cache from the http
    /// json-rpc-v2 senders inherited from protocol_bitcoind_rpc). The
    /// close_reason overloads complete via handle_complete's (ec, reason)
    /// idiom: the channel only stops (if reason is truthy) once the write
    /// has actually completed, so a failed authenticate's error reaches the
    /// client before the connection closes.
    void send_btcd_result(network::rpc::value_option&& result,
        size_t size_hint) NOEXCEPT;
    void send_btcd_error(const code& ec) NOEXCEPT;
    void send_btcd_error(const code& ec, size_t size_hint) NOEXCEPT;
    void send_btcd_error(const code& ec, size_t size_hint,
        const code& close_reason) NOEXCEPT;
    void send_btcd_notification(const std::string& method,
        network::rpc::array_t&& params, size_t size_hint) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void btcd_subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        btcd_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // Senders.
    void send_btcd_rpc(network::rpc::response_t&& model, size_t size_hint,
        const code& close_reason) NOEXCEPT;

    // These are thread safe.
    std::atomic_bool subscribed_blocks_{};
    const options_t& options_;
    const channel_btcd::ptr btcd_channel_;
    const uint8_t p2kh_;
    const uint8_t p2sh_;

    // These are protected by strand.
    btcd_dispatcher btcd_dispatcher_{};
    network::rpc::version btcd_version_{};
    network::rpc::id_option btcd_id_{};

    // Tx filter (loadtxfilter), protected by strand -- populated by
    // handle_load_tx_filter, read/mutated by match_filtered_transactions
    // (do_block_connected/do_block_disconnected, handle_rescan_blocks), all
    // of which run stranded (ws dispatch and POST_BTCD-posted chase handlers
    // alike). p2kh_/p2wpkh_ share one hash set (both 20-byte hash160 payloads
    // but under different script templates, kept separate so a p2kh watch
    // never matches a p2wpkh output or vice versa); p2sh_/p2wsh_ likewise.
    std::unordered_set<system::short_hash> pay_key_hashes_{};
    std::unordered_set<system::short_hash> pay_script_hashes_{};
    std::set<system::data_chunk> pay_witness_key_hash_programs_{};
    std::set<system::data_chunk> pay_witness_script_hash_programs_{};
    std::set<system::data_chunk> pay_witness_taproot_programs_{};
    std::unordered_set<system::chain::point> filter_outpoints_{};
};

} // namespace server
} // namespace libbitcoin

#endif
