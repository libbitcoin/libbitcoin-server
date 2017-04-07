/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/workers/authenticator.hpp>

#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

authenticator::authenticator(server_node& node)
  : zmq::authenticator(priority(node.server_settings().priority))
{
    const auto& settings = node.server_settings();

    set_private_key(settings.server_private_key);

    // Secure clients are also affected by address restrictions.
    for (const auto& public_key: settings.client_public_keys)
    {
        LOG_DEBUG(LOG_SERVER)
            << "Allow client public key [" << public_key << "]";

        allow(public_key);
    }

    // Allow wins in case of conflict with deny (first writer).
    for (const auto& address: settings.client_addresses)
    {
        LOG_DEBUG(LOG_SERVER)
            << "Allow client address [" << address.to_hostname() << "]";

        // The port is ignored.
        allow(address);
    }

    // Allow wins in case of conflict with deny (first writer).
    for (const auto& address: settings.blacklists)
    {
        LOG_DEBUG(LOG_SERVER)
            << "Block client address [" << address.to_hostname() << "]";

        // The port is ignored.
        deny(address);
    }
}

bool authenticator::apply(zmq::socket& socket, const std::string& domain,
    bool secure)
{
    // This will fail if there are client keys but no server key.
    if (!zmq::authenticator::apply(socket, domain, secure))
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to apply authentication to socket [" << domain << "]";
        return false;
    }

    if (secure)
    {
        LOG_DEBUG(LOG_SERVER)
            << "Applied curve authentication to socket [" << domain << "]";
    }
    else
    {
        LOG_DEBUG(LOG_SERVER)
            << "Applied address authentication to socket [" << domain << "]";
    }

    return true;
}

} // namespace server
} // namespace libbitcoin
