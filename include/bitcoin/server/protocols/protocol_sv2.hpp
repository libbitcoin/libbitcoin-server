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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SV2_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SV2_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

/// Stratum v2 service protocol, common shape of a service over
/// channel_stratum_v2, attached once the connection is set up.
class protocol_sv2
  : public server::protocol,
    public network::protocol
{
public:
    using options_t = network::settings::websocket_server;

protected:
    inline protocol_sv2(const auto& session,
        const network::channel::ptr& channel, const options_t&) NOEXCEPT
      : server::protocol(session, channel),
        network::protocol(session, channel)
    {
    }
};

} // namespace server
} // namespace libbitcoin

#endif
