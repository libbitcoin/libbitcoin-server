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
#ifndef LIBBITCOIN_SERVER_SESSIONS_SESSION_HPP
#define LIBBITCOIN_SERVER_SESSIONS_SESSION_HPP

#include <memory>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

/// Intermediate base class for future server injection.
class session
  : public node::session,
    protected network::tracker<session>
{
public:
    typedef std::shared_ptr<session> ptr;

    /// Construct an instance (network should be started).
    inline session(server_node& node, const configuration& config) NOEXCEPT
      : node::session((node::full_node&)node), config_(config),
        network::tracker<session>((network::net&)node)
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
    // This is thread safe.
    const configuration& config_;
};

} // namespace server
} // namespace libbitcoin

#endif
