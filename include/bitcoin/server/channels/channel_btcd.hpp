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

/// Channel for btcd channels (http/ws json-rpc).
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
        network::tracker<channel_btcd>(log),
        options_(options)
    {
    }

    /// True once the in-band 'authenticate' call has succeeded.
    inline bool authenticated() const NOEXCEPT
    {
        return authenticated_;
    }

    /// Latch the credential digest satisfying 'authenticate', applying its
    /// method scoping to subsequent ws traffic (see permitted).
    inline void set_authenticated(const system::hash_digest& digest) NOEXCEPT
    {
        authenticated_ = true;
        authenticated_digest_ = digest;
    }

    /// True if the credential that authorized this channel permits the rpc
    /// method. A ws connection is authorized by basic auth on the upgrade
    /// request or by the in-band 'authenticate' call (btcd allows either).
    inline bool permitted(const std::string& method) const NOEXCEPT override
    {
        if (!websocket())
            return network::channel_http::permitted(method);

        if (!options_.authorize())
            return true;

        if (network::channel_http::permitted(method))
            return true;

        return authenticated_ && options_.permitted(authenticated_digest_, method);
    }

protected:
    using value_type = network::http::body::value_type;

    /// Overridden to change default websocket reader to json-rpc request.
    inline value_type websocket_body() const NOEXCEPT override
    {
        // There is no forwarding constructor so assign and move.
        network::http::body::value_type value{};
        value = network::rpc::request{};
        return value;
    }

    /// ws data frames cannot carry an Authorization header, so the base
    /// check would reject them all once a credential is configured --
    /// permitted() is the per-method gate for ws traffic. The plain http
    /// post path keeps normal per-request enforcement.
    inline bool authorized() const NOEXCEPT override
    {
        return websocket() ? true : network::channel_http::authorized();
    }

private:
    // This is thread safe.
    const options_t& options_;

    // These are protected by strand.
    bool authenticated_{};
    system::hash_digest authenticated_digest_{};
};

} // namespace server
} // namespace libbitcoin

#endif
