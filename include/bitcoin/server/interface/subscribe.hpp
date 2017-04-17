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
#ifndef LIBBITCOIN_SERVER_SUBSCRIBE_HPP
#define LIBBITCOIN_SERVER_SUBSCRIBE_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

/// Subscribe interface.
/// Class and method names are published and mapped to the zeromq interface.
class BCS_API subscribe
{
public:
    /// Subscribe to payment address notifications by address hash.
    static void address(server_node& node, const message& request,
        send_handler handler);

    /// Subscribe to stealth address notifications by prefix.
    static void stealth(server_node& node, const message& request,
        send_handler handler);
};

} // namespace server
} // namespace libbitcoin

#endif
