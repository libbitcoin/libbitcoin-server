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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BITCOIND_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BITCOIND_HPP

#include <bitcoin/server/channels/channel_rpc.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Channel for the bitcoind service (universal json-rpc).
class BCS_API channel_bitcoind
  : public channel_rpc,
    protected network::tracker<channel_bitcoind>
{
public:
    typedef std::shared_ptr<channel_bitcoind> ptr;

    inline channel_bitcoind(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options,
        bool in_band=false) NOEXCEPT
      : channel_rpc(log, socket, identifier, config, options, in_band),
        network::tracker<channel_bitcoind>(log)
    {
    }
};

} // namespace server
} // namespace libbitcoin

#endif
