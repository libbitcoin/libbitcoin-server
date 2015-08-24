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
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/config/settings_type.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/fetch_x.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::blockchain;
using namespace bc::node;
using std::placeholders::_1;
using std::placeholders::_2;

const settings_type server_node::defaults
{
    // [server]
    bc::server::settings
    {
        SERVER_QUERY_ENDPOINT,
        SERVER_HEARTBEAT_ENDPOINT,
        SERVER_BLOCK_PUBLISH_ENDPOINT,
        SERVER_TRANSACTION_PUBLISH_ENDPOINT,
        SERVER_PUBLISHER_ENABLED,
        SERVER_QUERIES_ENABLED,
        SERVER_LOG_REQUESTS,
        SERVER_POLLING_INTERVAL_MILLISECONDS,
        SERVER_HEARTBEAT_INTERVAL_SECONDS,
        SERVER_SUBSCRIPTION_EXPIRATION_MINUTES,
        SERVER_SUBSCRIPTION_LIMIT,
        SERVER_CERTIFICATE_FILE,
        SERVER_CLIENT_CERTIFICATES_PATH,
        SERVER_WHITELISTS
    },

    // [node]
    bc::node::settings
    {
        NODE_THREADS,
        NODE_TRANSACTION_POOL_CAPACITY,
        NODE_PEERS,
        NODE_BLACKLISTS
    },

    // [blockchain]
    bc::chain::settings
    {
        BLOCKCHAIN_THREADS,
        BLOCKCHAIN_BLOCK_POOL_CAPACITY,
        BLOCKCHAIN_HISTORY_START_HEIGHT,
        BLOCKCHAIN_DATABASE_PATH,
        BLOCKCHAIN_CHECKPOINTS
    },

    // [network]
    bc::network::settings
    {
        NETWORK_THREADS,
        NETWORK_INBOUND_PORT,
        NETWORK_INBOUND_CONNECTION_LIMIT,
        NETWORK_OUTBOUND_CONNECTIONS,
        NETWORK_CONNECT_TIMEOUT_SECONDS,
        NETWORK_CHANNEL_HANDSHAKE_SECONDS,
        NETWORK_CHANNEL_REVIVAL_MINUTES,
        NETWORK_CHANNEL_HEARTBEAT_MINUTES,
        NETWORK_CHANNEL_INACTIVITY_MINUTES,
        NETWORK_CHANNEL_EXPIRATION_MINUTES,
        NETWORK_CHANNEL_GERMINATION_SECONDS,
        NETWORK_HOST_POOL_CAPACITY,
        NETWORK_RELAY_TRANSACTIONS,
        NETWORK_HOSTS_FILE,
        NETWORK_DEBUG_FILE,
        NETWORK_ERROR_FILE,
        NETWORK_SELF,
        NETWORK_SEEDS
    }
};

server_node::server_node(const settings_type& config)
  : full_node(config),
    retry_start_timer_(memory_threads_.service()),
    minimum_start_height_(config.minimum_start_height())
{
}

bool server_node::start(const settings_type& config)
{
    return full_node::start(config);
}

void server_node::subscribe_blocks(block_notify_callback notify_block)
{
    block_sunscriptions_.push_back(notify_block);
}

void server_node::subscribe_transactions(transaction_notify_callback notify_tx)
{
    tx_subscriptions_.push_back(notify_tx);
}

void server_node::new_unconfirm_valid_tx(const std::error_code& ec,
    const chain::index_list& unconfirmed, const chain::transaction& tx)
{
    full_node::new_unconfirm_valid_tx(ec, unconfirmed, tx);

    if (ec == bc::error::service_stopped)
        return;

    // Fire server protocol tx subscription notifications.
    for (const auto notify: tx_subscriptions_)
        notify(tx);
}

void server_node::broadcast_new_blocks(const std::error_code& ec,
    uint32_t fork_point,
    const bc::blockchain::blockchain::block_list& new_blocks,
    const bc::blockchain::blockchain::block_list& replaced_blocks)
{
    broadcast_new_blocks(ec, fork_point, new_blocks, replaced_blocks);

    if (ec == bc::error::service_stopped)
        return;

    if (fork_point < minimum_start_height_)
        return;

    // Fire server protocol block subscription notifications.
    for (auto new_block: new_blocks)
    {
        const size_t height = ++fork_point;
        for (const auto notify: block_sunscriptions_)
            notify(height, *new_block);
    }
}

void server_node::fullnode_fetch_history(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    uint32_t from_height;
    wallet::payment_address address;

    if (!unwrap_fetch_history_args(address, from_height, request))
        return;

    const auto handler = 
        std::bind(send_history_result,
            _1, _2, request, queue_send);

    fetch_history(node.blockchain(), node.transaction_indexer(), address,
        handler, from_height);
}

} // namespace server
} // namespace libbitcoin
