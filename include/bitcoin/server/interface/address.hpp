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
#ifndef LIBBITCOIN_SERVER_ADDRESS_HPP
#define LIBBITCOIN_SERVER_ADDRESS_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/address_notifier.hpp>

namespace libbitcoin {
namespace server {

/// Address interface.
/// Class and method names are published and mapped to the zeromq interface.
class BCS_API address
{
public:
    /// Fetch the blockchain and transaction pool history of a payment address.
    static void fetch_history2(server_node& node,
        const incoming& request, send_handler handler);

    /// Subscribe to payment and stealth address notifications by prefix.
    static void subscribe(address_notifier& notifier,
        const incoming& request, send_handler handler);

    /// Subscribe to payment and stealth address notifications by prefix.
    static void renew(address_notifier& notifier,
        const incoming& request, send_handler handler);
};

} // namespace server
} // namespace libbitcoin

#endif
