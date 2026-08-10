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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_DISPATCH_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_DISPATCH_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind.hpp>

namespace libbitcoin {
namespace server {

/// Interface subgroup dispatch, the common shape of the bitcoind subgroup
/// protocols. Carries the subgroup interface dispatcher and claims each
/// request defined by the interface, silently deferring all others to the
/// terminal responder (protocol_bitcoind, attached last). All subgroups are
/// explicitly instantiated (with their dispatchers) in the implementation
/// translation unit, isolating the dispatch metaprogramming there.
template <typename Interface>
class protocol_bitcoind_dispatch
  : public protocol_bitcoind
{
public:
    using rpc_dispatcher = network::rpc::dispatcher<Interface>;

    /// Subscribe to the post and websocket transports of the interface.
    void start() NOEXCEPT override;

    void stopping(const code& ec) NOEXCEPT override;

protected:
    inline protocol_bitcoind_dispatch(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind(session, channel, options)
    {
    }

    /// The post transport of the interface subgroup.
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override;

    /// The websocket transport of the interface subgroup.
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Subgroup handler wiring (dispatcher subscription).
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        rpc_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // This is protected by strand.
    rpc_dispatcher rpc_dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
