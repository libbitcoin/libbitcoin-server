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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_HPP

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind.hpp>

namespace libbitcoin {
namespace server {

/// btcd interface, added to the inherited bitcoind interface.
class BCS_API protocol_btcd
  : public server::protocol_bitcoind,
    protected network::tracker<protocol_btcd>
{
public:
    // Replace base class channel_t (authenticate authorizes in-band).
    using channel_t = channel_http<network::rpc::request, true>;

    typedef std::shared_ptr<protocol_btcd> ptr;
    using btcd_interface = interface::btcd;
    using btcd_dispatcher = network::rpc::dispatcher<btcd_interface>;

    inline protocol_btcd(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_bitcoind(session, channel, options),
        network::tracker<protocol_btcd>(session->log),
        options_(options),
        turbo_(session->database_settings().turbo),
        notification_strand_(channel->service().get_executor())
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Dispatch.
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Handlers (administrative).
    bool handle_authenticate(const code& ec, btcd_interface::authenticate,
        const std::string& username, const std::string& password) NOEXCEPT;
    bool handle_session(const code& ec, btcd_interface::session) NOEXCEPT;
    bool handle_stop(const code& ec, btcd_interface::stop) NOEXCEPT;

    /// Handlers (getters).
    bool handle_get_best_block(const code& ec,
        btcd_interface::get_best_block) NOEXCEPT;
    bool handle_get_current_net(const code& ec,
        btcd_interface::get_current_net) NOEXCEPT;
    bool handle_get_difficulty(const code& ec,
        btcd_interface::get_difficulty) NOEXCEPT;
    bool handle_get_info(const code& ec, btcd_interface::get_info) NOEXCEPT;
    bool handle_get_net_totals(const code& ec,
        btcd_interface::get_net_totals) NOEXCEPT;

    /// Handlers (subscription).
    bool handle_notify_blocks(const code& ec,
        btcd_interface::notify_blocks) NOEXCEPT;
    bool handle_stop_notify_blocks(const code& ec,
        btcd_interface::stop_notify_blocks) NOEXCEPT;
    bool handle_notify_new_transactions(const code& ec,
        btcd_interface::notify_new_transactions, bool verbose) NOEXCEPT;
    bool handle_stop_notify_new_transactions(const code& ec,
        btcd_interface::stop_notify_new_transactions) NOEXCEPT;

    /// Handlers (filters).
    bool handle_load_tx_filter(const code& ec,
        btcd_interface::load_tx_filter, bool reload,
        const network::rpc::value_t& addresses,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_rescan_blocks(const code& ec,
        btcd_interface::rescan_blocks,
        const network::rpc::value_t& blockhashes) NOEXCEPT;

    /// Handlers (deprecated, not_implemented).
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

    /// Event handlers.
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;

    /// Sender (server push, no id).
    void send_notification(const std::string& method,
        network::rpc::array_t&& params, size_t size_hint) NOEXCEPT;

protected:
    using point = system::chain::point;
    using hash_digest = system::hash_digest;
    using header_cptr = system::chain::header::cptr;
    using hashes_ptr = std::shared_ptr<system::hashes>;
    using array_ptr = std::shared_ptr<network::rpc::array_t>;
    using history = database::history;
    using histories = database::histories;
    using cursor_t = database::height_link;

    struct address_watch final
    {
        cursor_t cursor{};
    };

    struct outpoint_watch final
    {
        database::history outpoint{};
        database::histories spenders{};
    };

    /// Completion handlers (for long-running or other async queries).
    /// -----------------------------------------------------------------------

    void do_load_tx_filter(bool reload, const system::hashes& keys,
        const system::chain::points& points) NOEXCEPT;
    void complete_load_tx_filter(const code& ec) NOEXCEPT;

    void do_rescan_blocks(const hashes_ptr& hashes) NOEXCEPT;
    void do_rescan_watches(const hashes_ptr& hashes,
        const system::hashes& keys, system::chain::points& points) NOEXCEPT;
    void complete_rescan_blocks(const code& ec,
        const array_ptr& discovered) NOEXCEPT;

    /// Notification event handlers.
    /// -----------------------------------------------------------------------

    void do_connected(node::header_t link) NOEXCEPT;
    void do_disconnected(node::header_t link) NOEXCEPT;
    void notify_connected(const header_cptr& header, size_t height,
        const array_ptr& txs) NOEXCEPT;
    void notify_disconnected(const header_cptr& header,
        size_t height) NOEXCEPT;

    /// Utilities.
    /// -----------------------------------------------------------------------
    using matched_txs = std::map<size_t, hash_digest>;
    using matches = std::map<size_t, matched_txs>;
    using sizes = std::set<size_t>;

    code match_addresses(matches& out, address_watch& sub,
        const hash_digest& key, const sizes& heights) NOEXCEPT;
    void match_outpoints(matches& out, outpoint_watch& sub,
        const point& prevout, const sizes& heights) NOEXCEPT;
    network::rpc::array_t serialize_matches(const matched_txs& txs) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void btcd_subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        btcd_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // Post to notification strand.
    template <class Derived, typename Method, typename... Args>
    inline auto notify(Method&& method, Args&&... args) NOEXCEPT
    {
        return boost::asio::post(notification_strand_,
            BIND_SAFE(BIND_SHARED(method, args)));
    }

    // These are thread safe.
    const options_t& options_;
    const bool turbo_;
    std::atomic_bool stopping_{};
    std::atomic_bool subscribed_blocks_{};

    // This is protected by strand.
    btcd_dispatcher btcd_dispatcher_{};

    // This is thread safe, uses network threadpool.
    network::asio::strand notification_strand_;

    // These are protected by notification strand.
    std::map<point, outpoint_watch> outpoint_watches_{};
    std::map<hash_digest, address_watch> address_watches_{};
};

} // namespace server
} // namespace libbitcoin

#endif
