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
#ifndef LIBBITCOIN_SERVER_SESSIONS_SESSION_SERVER_HPP
#define LIBBITCOIN_SERVER_SESSIONS_SESSION_SERVER_HPP

#include <memory>
#include <utility>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/sessions/session.hpp>

namespace libbitcoin {
namespace server {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

class server_node;

/// Declare a concrete instance of this type for client-server protocols built
/// on tcp/ip. session_base processing performs all connection management and
/// session tracking. This includes start/stop/disable/enable/black/whitelist.
/// First protocol must declare options_t and channel_t. Each of the protocols
/// are constructed and attached to a constructed instance of channel_t. The
/// protocol construct and attachment can be overridden and/or augmented with
/// other protocols.
template <typename ...Protocols>
class session_server
  : public server::session,
    public network::session_server,
    protected network::tracker<session_server<Protocols...>>
{
public:
    typedef std::shared_ptr<session_server<Protocols...>> ptr;

    /// Extract the first protocol type from the pack.
    template <typename Protocol, typename...>
    struct first_protocol { using type = Protocol; };

    /// The first protocol must define these public types.
    using first = typename first_protocol<Protocols...>::type;
    using options_t = typename first::options_t;
    using channel_t = typename first::channel_t;

    /// Construct an instance (network should be started).
    inline session_server(server_node& node, uint64_t identifier,
        const configuration& config, const options_t& options) NOEXCEPT
      : server::session(node, config),
        network::session_server((network::net&)node, identifier, options),
        options_(options),
        network::tracker<session_server<Protocols...>>(node)
    {
    }

protected:
    using socket_ptr = network::socket::ptr;
    using channel_ptr = network::channel::ptr;

    /// Inbound connection attempts are dropped unless confirmed is current.
    /// Used instead of suspension because that has independent start/stop.
    inline bool enabled() const NOEXCEPT override
    {
        // Currently delay_inbound is the only reason to inherit node::session.
        return !this->node_config().node.delay_inbound || this->is_recent();
    }

    /// Override to construct channel. This allows the implementation to pass
    /// other values to protocol construction and/or select the desired channel
    /// based on available factors (e.g. a distinct protocol version).
    inline channel_ptr create_channel(
        const socket_ptr& socket) NOEXCEPT override
    {
        const auto channel = std::make_shared<channel_t>(log, socket,
            this->create_key(), this->node_config(), this->options_);

        return std::static_pointer_cast<network::channel>(channel);
    }

    template <typename ...Rest, bool_if<is_zero(sizeof...(Rest))> = true>
    inline void attach_rest(const channel_ptr&, const ptr&) NOEXCEPT{}

    template <typename Next, typename ...Rest>
    inline void attach_rest(const channel_ptr& channel, const ptr& self) NOEXCEPT
    {
        channel->template attach<Next>(self, this->options_)->start();
        attach_rest<Rest...>(channel, self);
    }

    /// Overridden to set channel protocols. This allows the implementation to
    /// pass other values to protocol construction and/or select the desired
    /// protocol based on available factors (e.g. a distinct protocol version).
    /// Use std::dynamic_pointer_cast<channel_t>(channel) to obtain channel_t.
    inline void attach_protocols(const channel_ptr& channel) NOEXCEPT override
    {
        using own = session_server<Protocols...>;
        const auto self = this->template shared_from_base<own>();
        attach_rest<Protocols...>(channel, self);
    }

protected:
    // This is thread safe.
    const options_t& options_;
};

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin

#endif
