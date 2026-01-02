/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_RPC_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_RPC_HPP

#include <memory>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

/// Abstract base for RPC protocols, thread safe.
template <typename Channel>
class BCS_API protocol_rpc
  : public server::protocol,
    public network::protocol_rpc<Channel>,
    protected network::tracker<protocol_rpc<Channel>>
{
public:
    using channel_t = Channel;
    using options_t = channel_t::options_t;

protected:
    inline protocol_rpc(const auto& session,
        const network::channel::ptr& channel, const options_t& options) NOEXCEPT
      : server::protocol(session, channel),
        network::protocol_rpc<Channel>(session, channel, options),
        network::tracker<protocol_rpc<Channel>>(session->log)
    {
    }
};

#define SUBSCRIBE_RPC(...) SUBSCRIBE_CHANNEL(void, __VA_ARGS__)
#define SEND_RPC(message, size_hint, method, ...) \
    send<CLASS>(message, size_hint, &CLASS::method, __VA_ARGS__)

} // namespace server
} // namespace libbitcoin

#endif
