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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_STRATUM_V1_HPP

#include <memory>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

/// Channel for stratum v1 channels (non-http json-rpc).
class BCS_API channel_stratum_v1
  : public node::channel,
    public network::channel_rpc<interface::stratum_v1>,
    protected network::tracker<channel_stratum_v1>
{
public:
    typedef std::shared_ptr<channel_stratum_v1> ptr;
    using interface_t = interface::stratum_v1;
    using options_t = typename network::channel_rpc<interface_t>::options_t;

    inline channel_stratum_v1(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : node::channel(log, socket, identifier, config),
        network::channel_rpc<interface::stratum_v1>(log, socket, identifier,
            config.network, options),
        network::tracker<channel_stratum_v1>(log)
    {
    }
};

} // namespace server
} // namespace libbitcoin

#endif
