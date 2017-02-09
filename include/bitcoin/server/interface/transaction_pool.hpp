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
#ifndef LIBBITCOIN_SERVER_TRANSACTION_POOL_HPP
#define LIBBITCOIN_SERVER_TRANSACTION_POOL_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

/// Transaction pool interface.
/// Class and method names are published and mapped to the zeromq interface.
class BCS_API transaction_pool
{
public:
    /// Save to tx pool and announce to all connected peers.
    static void broadcast(server_node& node, const message& request,
        send_handler handler);

    /// Fetch a transaction from the transaction pool (or chain), by its hash.
    static void fetch_transaction(server_node& node, const message& request,
        send_handler handler);

    /// Validate a transaction against the transaction pool and blockchain.
    static void validate2(server_node& node, const message& request,
        send_handler handler);

private:
    static void handle_broadcast(const code& ec, const message& request,
        send_handler handler);

    static void handle_validated2(const code& ec, const message& request,
        send_handler handler);
};

} // namespace server
} // namespace libbitcoin

#endif
