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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BTCD_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BTCD_HPP

#include <bitcoin/server/channels/channel_bitcoind.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Channel for the btcd service, as bitcoind but authorized in-band, so
/// the websocket upgrade is open (authenticate follows it).
class BCS_API channel_btcd
  : public channel_bitcoind
{
public:
    typedef std::shared_ptr<channel_btcd> ptr;

    inline channel_btcd(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : channel_bitcoind(log, socket, identifier, config, options, true)
    {
    }
};

} // namespace server
} // namespace libbitcoin

#endif
