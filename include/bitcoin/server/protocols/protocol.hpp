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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_HPP

#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>

// Only session.hpp.
#include <bitcoin/server/sessions/session.hpp>

namespace libbitcoin {
namespace server {

/// Abstract base server protocol.
class BCS_API protocol
  : public node::protocol,
    protected network::tracker<protocol>
{
public:
    typedef std::shared_ptr<protocol> ptr;
    using gate_t = network::channel::gate_t;

    /// Connections are dropped until the confirmed chain is current.
    static constexpr bool delayed{ true };

    /// The maximum number of txs in a submitted package (bitcoind).
    static constexpr size_t maximum_package{ 25 };

    inline protocol(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol>(session->log),
        config_(session->server_config()),
        session_(session),
        channel_(channel)
    {
    }

    /// Configuration settings for all server libraries.
    inline const configuration& server_config() const NOEXCEPT
    {
        return config_;
    }

    /// Server config settings.
    inline const settings& server_settings() const NOEXCEPT
    {
        return server_config().server;
    }

    /// The number of host pool addresses by address type.
    inline network::config::address_counts address_counts() const NOEXCEPT
    {
        return session_->address_counts();
    }

    /// Get a randomized subset of pooled addresses.
    inline void dump_addresses(
        network::address_handler&& handler) const NOEXCEPT
    {
        session_->dump_addresses(std::move(handler));
    }

    /// Get the closed channel byte totals with the active identifiers.
    inline void fetch_totals(
        network::net::totals_handler&& handler) const NOEXCEPT
    {
        session_->fetch_totals(std::move(handler));
    }

    /// Count a channel demand for wire tx|block broadcasts (or uncount).
    inline void demand_wire_tx(bool demand) const NOEXCEPT
    {
        session_->demand_wire_tx(demand);
    }

    inline void demand_wire_block(bool demand) const NOEXCEPT
    {
        session_->demand_wire_block(demand);
    }

    /// Suspend all existing and future network connections.
    inline void fault(const code& ec) const NOEXCEPT
    {
        session_->fault(ec);
    }

    /// Subscribe to channel stop, key for unsubscribe (requires strand).
    inline network::proxy::object_key subscribe_stop(
        network::result_handler&& handler) const NOEXCEPT
    {
        return channel_->subscribe_stop(std::move(handler));
    }

    /// Unsubscribe from channel stop (requires strand).
    inline bool unsubscribe_stop(
        network::proxy::object_key key) const NOEXCEPT
    {
        return channel_->unsubscribe_stop(key);
    }

    /// Subscribe to chaser events, key to completer (any number per protocol).
    inline void subscribe_events(node::event_notifier&& handler,
        node::event_completer&& complete) const NOEXCEPT
    {
        session_->subscribe_chase(std::move(handler), std::move(complete));
    }

    /// Unsubscribe from chaser events, handler invoked with service_stopped.
    inline void unsubscribe_events(node::object_key key) const NOEXCEPT
    {
        session_->unsubscribe_chase(key);
    }

    /// Allocate a coinbase extranonce prefix, empty if exhausted.
    inline extranonces::prefix allocate_prefix(size_t size) const NOEXCEPT
    {
        return session_->allocate_prefix(size);
    }

    /// The mining channels of the stratum services.
    inline const miners::ptr& mining() const NOEXCEPT
    {
        return session_->mining();
    }

    /// Create a timer on the channel strand.
    inline network::deadline::ptr make_timer() const NOEXCEPT
    {
        return std::make_shared<network::deadline>(channel_->log,
            channel_->strand());
    }

    /// Height at and below which blocks may be pruned, zero if none.
    size_t get_pruned_height() const NOEXCEPT;

private:
    size_t get_active_height(const system::hash_digest& hash) const NOEXCEPT;

    // These are thread safe.
    const configuration& config_;
    const session::ptr session_;

    // This is mostly thread safe, and used in a thread safe manner.
    const network::channel::ptr channel_;
};

} // namespace server
} // namespace libbitcoin

// For use with secondary (e.g. notification) strands.
#define POST_NOTIFY(method, ...) notify<CLASS>(&CLASS::method, __VA_ARGS__)

// Qualified, as http protocols alias post as the request type.
#undef POST
#undef PARALLEL
#define POST(method, ...) \
    network::protocol::post<CLASS>(&CLASS::method __VA_OPT__(,) __VA_ARGS__)
#define PARALLEL(method, ...) \
    network::protocol::parallel<CLASS>(&CLASS::method __VA_OPT__(,) __VA_ARGS__)

#endif
