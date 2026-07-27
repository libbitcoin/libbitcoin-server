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

#include <memory>
#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class BCS_API channel_btcd
  : public server::channel,
    public network::channel_http,
    protected network::tracker<channel_btcd>
{
public:
    typedef std::shared_ptr<channel_btcd> ptr;

    inline channel_btcd(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_http(log, socket, identifier, config.network, options),
        network::tracker<channel_btcd>(log)
    {
    }

    /// True once the client has authenticated over the ws 'authenticate'
    /// handshake (or basic auth was not configured, requiring none).
    inline bool authenticated() const NOEXCEPT
    {
        return authenticated_;
    }

    inline void set_authenticated(bool value) NOEXCEPT
    {
        authenticated_ = value;
    }

protected:
    using value_type = network::http::body::value_type;

    /// Preselect the json-rpc request parser for websocket frames, so that
    /// each btcd ws message is parsed the same way as an http post body.
    inline value_type websocket_body() const NOEXCEPT override
    {
        // There is no forwarding constructor so assign and move.
        network::http::body::value_type value{};
        value = network::rpc::request{};
        return value;
    }

    /// Basic auth is a per-http-request header, which websocket data frames
    /// structurally cannot carry (only the upgrade handshake has headers,
    /// and that request never reaches this check -- see channel_http's
    /// error::upgraded short-circuit). Once upgraded, auth is instead
    /// enforced in-band by the ws 'authenticate' method (see
    /// protocol_btcd_rpc::dispatch_websocket); the plain http post path
    /// (e.g. inherited chain methods) keeps the normal per-request check.
    inline bool unauthorized(const network::http::request& request) NOEXCEPT override
    {
        return websocket() ? false : network::channel_http::unauthorized(request);
    }

private:
    // This is protected by strand.
    bool authenticated_{};
};

} // namespace server
} // namespace libbitcoin

#endif
