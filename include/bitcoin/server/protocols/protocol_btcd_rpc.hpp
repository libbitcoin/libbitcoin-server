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
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

namespace libbitcoin {
namespace server {

/// btcd-compatible json-rpc-v1 over websocket. Inherits the standard chain
/// handlers (getblockcount, getblock, etc.) from protocol_bitcoind_rpc, which
/// remain reachable via the inherited http post path (unchanged). Adds the
/// btcd-only extension methods (session/notification/filter/admin) over
/// websocket via a second dispatcher (dispatch_websocket, below).
///
/// Attaches immediately on connect (session_btcd = session_server<
/// protocol_btcd_rpc>, no separate handshake protocol/stage) -- matching real
/// btcd's own model, which has no handshake either: HTTP Basic Auth is
/// checked synchronously per plain-http request (channel_http::authorized(),
/// unchanged from bitcoind), and ws 'authenticate' is just an ordinary
/// extension method that lets a client establish the same authorization
/// in-band, for the one transport (websocket data frames) that structurally
/// cannot carry a per-message Authorization header. See docs/btcd-endpoint.md
/// for why an earlier session_handshake<protocol_btcd_auth, protocol_btcd_rpc>
/// design (modeled on electrum/p2p's version negotiation) was reverted: those
/// protocols' handshakes always wait for a real message to complete, but
/// btcd's has no credential-required case to wait for -- modeling it on that
/// pattern introduced an async attach gap real btcd never has, which a plain
/// http client (no upgrade round-trip to mask the gap, unlike ws) could land
/// in before this class ever subscribed its handlers.
///
/// TODO (phase B): bridge the standard chain handlers into the ws dispatcher
/// too, so a single ws connection can also reach them (real btcd/btcwallet
/// clients expect this); requires reconciling protocol_bitcoind_rpc's
/// post-oriented private send path with this class's ws-oriented senders.
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
        btcd_channel_(std::dynamic_pointer_cast<channel_btcd>(channel))
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Dispatch btcd ws frames (standard chain requests dispatch normally
    /// over http post via the inherited handle_receive_post).
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

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

    /// Handlers (address/outpoint filtering, not_implemented pending phase B).
    bool handle_load_tx_filter(const code& ec,
        btcd_interface::load_tx_filter, bool reload,
        const network::rpc::value_t& addresses,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_rescan_blocks(const code& ec,
        btcd_interface::rescan_blocks,
        const network::rpc::value_t& blockhashes) NOEXCEPT;

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

    // These are protected by strand.
    btcd_dispatcher btcd_dispatcher_{};
    network::rpc::version btcd_version_{};
    network::rpc::id_option btcd_id_{};
};

} // namespace server
} // namespace libbitcoin

#endif
