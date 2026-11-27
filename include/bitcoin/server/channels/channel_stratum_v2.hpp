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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_STRATUM_V2_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_STRATUM_V2_HPP

#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Channel for stratum v2 service, served over ws/s.
class BCS_API channel_stratum_v2
  : public server::channel,
    public network::channel_sv2,
    protected network::tracker<channel_stratum_v2>
{
public:
    typedef std::shared_ptr<channel_stratum_v2> ptr;
    using options_t = settings::stratum_v2_server;

    inline channel_stratum_v2(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_sv2(log, socket, identifier, config.network, options),
        network::tracker<channel_stratum_v2>(log)
    {
    }

    /// The vendor and firmware of the miner (requires strand).
    inline void set_client(const std::string& client) NOEXCEPT
    {
        client_ = client;
    }

    inline const std::string& client() const NOEXCEPT
    {
        return client_;
    }

private:
    // This is protected by strand.
    std::string client_{};
};

} // namespace server
} // namespace libbitcoin

#endif
