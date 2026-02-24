/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_SESSIONS_SESSION_HANDSHAKE_HPP
#define LIBBITCOIN_SERVER_SESSIONS_SESSION_HANDSHAKE_HPP

#include <memory>
#include <utility>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/sessions/session_server.hpp>

namespace libbitcoin {
namespace server {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

class server_node;

/// This is session_server<> with added support for single handshake protocol.
template <typename Handshake, typename ...Protocols>
class session_handshake
  : public session_server<Protocols...>,
    protected network::tracker<session_handshake<Handshake, Protocols...>>
{
public:
    typedef std::shared_ptr<session_handshake<Handshake, Protocols...>> ptr;
    using base = session_server<Protocols...>;

    /// Construct an instance (network should be started).
    inline session_handshake(server_node& node, uint64_t identifier,
        const configuration& config, const base::options_t& options) NOEXCEPT
      : session_server<Protocols...>(node, identifier, config, options),
        network::tracker<session_handshake<Handshake, Protocols...>>(node)
    {
    }

protected:
    /// Overridden to implement a connection handshake. Handshake protocol(s)
    /// must invoke handler one time at completion. Use 
    /// std::dynamic_pointer_cast<channel_t>(channel) to obtain channel_t.
    inline void attach_handshake(const base::channel_ptr& channel,
        network::result_handler&& handler) NOEXCEPT override
    {
        using own = session_handshake<Handshake, Protocols...>;
        const auto self = this->template shared_from_base<own>();
        channel->template attach<Handshake>(self, this->options_)->
            shake(std::move(handler));
    }

    /// Enable handshake (network::session_server disables by default).
    inline void do_attach_handshake(const base::channel_ptr& channel,
        const network::result_handler& handshake) NOEXCEPT override
    {
        network::session::do_attach_handshake(channel, handshake);
    }
};

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin

#endif
