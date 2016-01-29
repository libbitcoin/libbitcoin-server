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
#ifndef LIBBITCOIN_SERVER_BLOCKCHAIN_HPP
#define LIBBITCOIN_SERVER_BLOCKCHAIN_HPP

#include <bitcoin/blockchain.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/message/incoming_message.hpp>
#include <bitcoin/server/message/outgoing_message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {
    
/// Blockchain interface.
/// Class and method names are published and mapped to the zeromq interface.
class BCS_API blockchain
{
public:
    static void fetch_history(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_transaction(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_last_height(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_block_header(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_block_transaction_hashes(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_transaction_index(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_spend(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_block_height(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_stealth(server_node& node,
        const incoming_message& request, send_handler handler);

private:
    static void last_height_fetched(const code& ec, size_t last_height,
        const incoming_message& request, send_handler handler);

    static void fetch_block_header_by_hash(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_block_header_by_height(server_node& node,
        const incoming_message& request, send_handler handler);

    static void block_header_fetched(const code& ec,
        const chain::header& block, const incoming_message& request,
        send_handler handler);

    static void fetch_block_transaction_hashes_by_hash(server_node& node,
        const incoming_message& request, send_handler handler);

    static void fetch_block_transaction_hashes_by_height(server_node& node,
        const incoming_message& request, send_handler handler);

    static void block_transaction_hashes_fetched(const code& ec,
        const hash_list& hashes, const incoming_message& request,
        send_handler handler);

    static void transaction_index_fetched(const code& ec, size_t block_height,
        size_t index, const incoming_message& request, send_handler handler);

    static void spend_fetched(const code& ec, 
        const chain::input_point& inpoint, const incoming_message& request,
        send_handler handler);

    static void block_height_fetched(const code& ec, size_t block_height,
        const incoming_message& request, send_handler handler);

    static void stealth_fetched(const code& ec,
        const bc::blockchain::block_chain::stealth& stealth_results,
        const incoming_message& request, send_handler handler);
};

} // namespace server
} // namespace libbitcoin

#endif
