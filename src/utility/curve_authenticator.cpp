/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/utility/curve_authenticator.hpp>

#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

curve_authenticator::curve_authenticator(server_node* node)
  : authenticator(node->thread_pool()),
    settings_(node->server_settings())
{
}

bool curve_authenticator::apply(zmq::socket& socket)
{
    const auto server_private_key = settings_.server_private_key;

    if (!server_private_key.empty())
    {
        // Server identification does not require the public key.
        if (!socket.set_private_key(server_private_key) ||
            !socket.set_curve_server())
        {
            log::error(LOG_SERVICE)
                << "Invalid server key.";
            return false;
        }

        log::info(LOG_SERVICE)
            << "Loaded server key.";
    }

    if (server_private_key.empty() && !settings_.client_public_keys.empty())
    {
        log::error(LOG_SERVICE)
            << "Client authentication requires a server key.";
        return false;
    }

    for (const auto& public_key: settings_.client_public_keys)
    {
        log::info(LOG_SERVICE)
            << "Allowed client [" << public_key << "]";

        allow(public_key);
    }

    for (const auto& address: settings_.client_addresses)
    {
        log::info(LOG_SERVICE)
            << "Allowed client [" << address
            << (address.port() == 0 ? ":*" : "") << "]";

        allow(address);
    }

    return true;
}

} // namespace server
} // namespace libbitcoin
