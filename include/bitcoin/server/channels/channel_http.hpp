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

#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Common base for http service channels. Service channel overrides
/// default_body to preselect its reader body, and a json-rpc body also implies
/// tcp downgrade detection. In-band service (e.g. btcd authenticate) authorizes
/// after upgrade, so its upgrade is open.
class BCS_API channel_http
  : public server::channel,
    public network::channel_http
{
public:
    typedef std::shared_ptr<channel_http> ptr;

    inline channel_http(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options,
        bool in_band=false) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_http(log, socket, identifier, config.network, options,
            in_band)
    {
    }

protected:
    using value_type = network::http::body::value_type;

    /// There is no forwarding constructor so assign and move.
    template <typename Body>
    static inline value_type to_body() NOEXCEPT
    {
        value_type value{};
        value = Body{};
        return value;
    }
};

} // namespace server
} // namespace libbitcoin

#endif
