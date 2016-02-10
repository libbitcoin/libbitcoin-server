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
#include <bitcoin/server/server_node.hpp>

#include <future>
#include <iostream>
#include <boost/filesystem.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>

namespace libbitcoin {
namespace server {
    
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::wallet;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

server_node::server_node(const configuration& configuration)
  : p2p_node(configuration),
    configuration_(configuration),
    last_checkpoint_height_(configuration.last_checkpoint_height())
{
}

// Properties.
// ----------------------------------------------------------------------------

const settings& server_node::server_settings() const
{
    return configuration_.server;
}

// Start sequence.
// ----------------------------------------------------------------------------

void server_node::start(result_handler handler)
{
    // Start the network and blockchain before subscribing.
    p2p_node::start(
        std::bind(&server_node::handle_node_start,
            shared_from_base<server_node>(), _1, handler));
}

void server_node::handle_node_start(const code& ec, result_handler handler)
{
    // Subscribe to blockchain reorganizations.
    subscribe_blockchain(
        std::bind(&server_node::handle_new_blocks,
            shared_from_base<server_node>(), _1, _2, _3, _4));

    // Subscribe to transaction pool acceptances.
    subscribe_transaction_pool(
        std::bind(&server_node::handle_tx_accepted,
            shared_from_base<server_node>(), _1, _2, _3));

    // This is the end of the derived start sequence.
    handler(error::success);
}

// This serves both address subscription and the block publisher.
void server_node::subscribe_blocks(block_notify_callback notify_block)
{
    block_sunscriptions_.push_back(notify_block);
}

// This serves both address subscription and the tx publisher.
void server_node::subscribe_transactions(transaction_notify_callback notify_tx)
{
    tx_subscriptions_.push_back(notify_tx);
}

bool server_node::handle_tx_accepted(const code& ec,
    const index_list& unconfirmed, const transaction& tx)
{
    if (ec == bc::error::service_stopped)
        return false;

    if (ec)
    {
        log::error(LOG_SERVICE)
            << "Failure handling new tx: " << ec.message();
        return false;
    }

    // Fire server protocol tx subscription notifications.
    for (const auto notify: tx_subscriptions_)
        notify(tx);

    return true;
}

bool server_node::handle_new_blocks(const code& ec, uint64_t fork_point,
    const block::ptr_list& new_blocks, const block::ptr_list& replaced_blocks)
{
    if (ec == bc::error::service_stopped)
        return false;

    if (fork_point < last_checkpoint_height_)
        return false;

    if (ec)
    {
        log::error(LOG_SERVICE)
            << "Failure handling new blocks: " << ec.message();
        return false;
    }

    BITCOIN_ASSERT(fork_point < max_uint32 - new_blocks.size());
    auto height = static_cast<uint32_t>(fork_point);

    // Fire server protocol block subscription notifications.
    for (auto new_block: new_blocks)
        for (const auto notify: block_sunscriptions_)
            notify(++height, new_block);

    return true;
}

} // namespace server
} // namespace libbitcoin
