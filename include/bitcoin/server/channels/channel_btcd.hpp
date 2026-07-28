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
        network::tracker<channel_btcd>(log),
        options_(options)
    {
    }

    /// True once the client has authenticated over the ws 'authenticate'
    /// handshake (or basic auth was not configured, requiring none).
    inline bool authenticated() const NOEXCEPT
    {
        return authenticated_;
    }

    /// Latch the credential digest that satisfied the in-band 'authenticate'
    /// call (see protocol_btcd_rpc::handle_authenticate), so that permitted()
    /// can later apply that credential's method scoping to ws traffic.
    inline void set_authenticated(const system::hash_digest& digest) NOEXCEPT
    {
        authenticated_ = true;
        authenticated_digest_ = digest;
    }

    /// True if the credential that authorized this channel permits the rpc
    /// method. Plain http traffic defers entirely to channel_http's own
    /// header-latched digest (unchanged from bitcoind). For ws traffic, real
    /// btcd recognizes two alternative ways a connection can be authorized --
    /// basic auth already established on the ws upgrade request (also
    /// channel_http's header-latched digest, since the upgrade request has
    /// real headers even though subsequent ws data frames don't), or the
    /// in-band 'authenticate' call (digest latched above); its own docs
    /// describe 'authenticate' as "disallowed when basic auth has already
    /// been established" -- i.e. an alternative to it, not an additional
    /// requirement on top of it.
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

    /// Preselect the json-rpc request parser for websocket frames, so that
    /// each btcd ws message is parsed the same way as an http post body.
    inline value_type websocket_body() const NOEXCEPT override
    {
        // There is no forwarding constructor so assign and move.
        network::http::body::value_type value{};
        value = network::rpc::request{};
        return value;
    }

    /// channel_http's authorized() only latches basic-auth from the ws
    /// upgrade request (the upgrade request has real headers); subsequent ws
    /// data frames are synthesized and structurally cannot carry a
    /// per-frame Authorization header. Deferring to the base check here
    /// would therefore reject every ws frame once a credential is
    /// configured, unless basic auth happened to be presented on the
    /// upgrade itself -- permitted() (above) is the real per-method gate for
    /// ws traffic, so this base check must never reject a ws frame on its
    /// own. The plain http post path (e.g. inherited bitcoind-compatible
    /// chain methods) keeps the normal per-request enforcement.
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
