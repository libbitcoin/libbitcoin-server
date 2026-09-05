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

#include <deque>
#include <memory>
#include <utility>
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

    inline protocol(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        config_(session->server_config()),
        network::tracker<protocol>(session->log),
        session_(session)
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

protected:
    /// A tx broadcast on this channel, with its identifier.
    using retained_t = std::pair<system::hash_digest,
        system::chain::transaction::cptr>;
    using retained_txs = std::deque<retained_t>;

    /// There is no tx pool, so a successfully-broadcast tx is retained on
    /// the channel that sent it, allowing that client to see it before it
    /// confirms. The buffer is bounded and dies with the channel.
    void retain_tx(const system::chain::transaction::cptr& tx) NOEXCEPT;

    /// Obtain a tx previously broadcast on this channel, or nullptr.
    system::chain::transaction::cptr retained_tx(
        const system::hash_digest& hash) const NOEXCEPT;

    /// The txs broadcast on this channel, oldest first.
    const retained_txs& retained() const NOEXCEPT;

private:
    // Bound on txs retained by one channel (evicted oldest first).
    static constexpr size_t maximum_retained = 16;

    // These are thread safe.
    const configuration& config_;
    const session::ptr session_;

    // This is protected by the channel strand.
    retained_txs retained_{};
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
