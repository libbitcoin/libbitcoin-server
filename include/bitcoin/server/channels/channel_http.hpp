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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_HTTP_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_HTTP_HPP

#include <memory>
#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Channel for http services, websocket frames read as Body (a json-rpc
/// service instantiates with network::rpc::request). An in-band service
/// (e.g. btcd authenticate) authorizes after upgrade, so its upgrade is open.
template <typename Body = network::http::string_value, bool InBand = false>
class BCS_API channel_http
  : public server::channel,
    public network::channel_http,
    protected network::tracker<channel_http<Body, InBand>>
{
public:
    typedef std::shared_ptr<channel_http> ptr;

    inline channel_http(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_http(log, socket, identifier, config.network, options,
            InBand),
        network::tracker<channel_http<Body, InBand>>(log)
    {
    }

protected:
    using value_type = network::http::body::value_type;

    /// Overridden to set the websocket reader body type.
    inline value_type websocket_body() const NOEXCEPT override
    {
        // There is no forwarding constructor so assign and move.
        value_type value{};
        value = Body{};
        return value;
    }
};

} // namespace server
} // namespace libbitcoin

#endif
