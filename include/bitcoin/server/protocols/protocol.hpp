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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_HPP

#include <memory>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Abstract base server protocol.
class BCS_API protocol
  : public node::protocol,
    protected network::tracker<protocol>
{
public:
    typedef std::shared_ptr<protocol> ptr;

    inline protocol(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        config_(session->server_config()),
        network::tracker<protocol>(session->log)
    {
    }

    /// Configuration settings for all server libraries.
    inline const configuration& server_config() const NOEXCEPT
    {
        return config_;
    }

    /// Server config settings.
    inline const settings& server_settings() const NOEXCEPT
    {
        return server_config().server;
    }

private:
    const configuration& config_;
};

} // namespace server
} // namespace libbitcoin

#endif
