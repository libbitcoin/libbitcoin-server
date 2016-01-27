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
#ifndef LIBBITCOIN_SERVER_TRANSACTION_POOL_HPP
#define LIBBITCOIN_SERVER_TRANSACTION_POOL_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// TODO: convert to class static.

void BCS_API transaction_pool_validate(server_node& node,
    const incoming_message& request, send_handler handler);

void BCS_API transaction_pool_fetch_transaction(server_node& node,
    const incoming_message& request, send_handler handler);

} // namespace server
} // namespace libbitcoin

#endif

