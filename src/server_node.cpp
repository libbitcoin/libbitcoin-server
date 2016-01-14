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
#include <bitcoin/server/config/configuration.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/fetch_x.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {
    
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::wallet;
using std::placeholders::_1;
using std::placeholders::_2;

static const configuration default_configuration()
{
    configuration defaults;
    defaults.node.threads = NODE_THREADS;
    defaults.node.transaction_pool_capacity = NODE_TRANSACTION_POOL_CAPACITY;
    defaults.node.peers = NODE_PEERS;
    defaults.chain.threads = BLOCKCHAIN_THREADS;
    defaults.chain.block_pool_capacity = BLOCKCHAIN_BLOCK_POOL_CAPACITY;
    defaults.chain.history_start_height = BLOCKCHAIN_HISTORY_START_HEIGHT;
    defaults.chain.use_testnet_rules = BLOCKCHAIN_TESTNET_RULES_MAINNET;
    defaults.chain.database_path = BLOCKCHAIN_DATABASE_PATH;
    defaults.chain.checkpoints = BLOCKCHAIN_CHECKPOINTS_MAINNET;
    defaults.network.threads = NETWORK_THREADS;
    defaults.network.identifier = NETWORK_IDENTIFIER_MAINNET;
    defaults.network.inbound_port = NETWORK_INBOUND_PORT_MAINNET;
    defaults.network.connection_limit = NETWORK_CONNECTION_LIMIT;
    defaults.network.outbound_connections = NETWORK_OUTBOUND_CONNECTIONS;
    defaults.network.manual_retry_limit = NETWORK_MANUAL_RETRY_LIMIT;
    defaults.network.connect_batch_size = NETWORK_CONNECT_BATCH_SIZE;
    defaults.network.connect_timeout_seconds = NETWORK_CONNECT_TIMEOUT_SECONDS;
    defaults.network.channel_handshake_seconds = NETWORK_CHANNEL_HANDSHAKE_SECONDS;
    defaults.network.channel_poll_seconds = NETWORK_CHANNEL_POLL_SECONDS;
    defaults.network.channel_heartbeat_minutes = NETWORK_CHANNEL_HEARTBEAT_MINUTES;
    defaults.network.channel_inactivity_minutes = NETWORK_CHANNEL_INACTIVITY_MINUTES;
    defaults.network.channel_expiration_minutes = NETWORK_CHANNEL_EXPIRATION_MINUTES;
    defaults.network.channel_germination_seconds = NETWORK_CHANNEL_GERMINATION_SECONDS;
    defaults.network.host_pool_capacity = NETWORK_HOST_POOL_CAPACITY;
    defaults.network.relay_transactions = NETWORK_RELAY_TRANSACTIONS;
    defaults.network.hosts_file = NETWORK_HOSTS_FILE;
    defaults.network.debug_file = NETWORK_DEBUG_FILE;
    defaults.network.error_file = NETWORK_ERROR_FILE;
    defaults.network.self = NETWORK_SELF;
    defaults.network.blacklists = NETWORK_BLACKLISTS;
    defaults.network.seeds = NETWORK_SEEDS_MAINNET;
    defaults.server.query_endpoint = SERVER_QUERY_ENDPOINT;
    defaults.server.heartbeat_endpoint = SERVER_HEARTBEAT_ENDPOINT;
    defaults.server.block_publish_endpoint = SERVER_BLOCK_PUBLISH_ENDPOINT;
    defaults.server.transaction_publish_endpoint = SERVER_TRANSACTION_PUBLISH_ENDPOINT;
    defaults.server.publisher_enabled = SERVER_PUBLISHER_ENABLED;
    defaults.server.queries_enabled = SERVER_QUERIES_ENABLED;
    defaults.server.log_requests = SERVER_LOG_REQUESTS;
    defaults.server.polling_interval_seconds = SERVER_POLLING_INTERVAL_SECONDS;
    defaults.server.heartbeat_interval_seconds = SERVER_HEARTBEAT_INTERVAL_SECONDS;
    defaults.server.subscription_expiration_minutes = SERVER_SUBSCRIPTION_EXPIRATION_MINUTES;
    defaults.server.subscription_limit = SERVER_SUBSCRIPTION_LIMIT;
    defaults.server.certificate_file = SERVER_CERTIFICATE_FILE;
    defaults.server.client_certificates_path = SERVER_CLIENT_CERTIFICATES_PATH;
    defaults.server.whitelists = SERVER_WHITELISTS;
    return defaults;
};

const configuration server_node::defaults = default_configuration();

server_node::server_node(const configuration& config)
  : full_node(config),
    configuration_(config),
    retry_start_timer_(memory_threads_.service()),
    last_checkpoint_height_(config.last_checkpoint_height())
{
}

void server_node::subscribe_blocks(block_notify_callback notify_block)
{
    block_sunscriptions_.push_back(notify_block);
}

void server_node::subscribe_transactions(transaction_notify_callback notify_tx)
{
    tx_subscriptions_.push_back(notify_tx);
}

void server_node::handle_tx_validated(const code& ec, const transaction& tx,
    const hash_digest& hash, const index_list& unconfirmed)
{
    full_node::handle_tx_validated(ec, tx, hash, unconfirmed);

    if (ec == bc::error::service_stopped)
        return;

    // Fire server protocol tx subscription notifications.
    for (const auto notify: tx_subscriptions_)
        notify(tx);
}

bool server_node::handle_new_blocks(const code& ec, uint64_t fork_point,
    const block_chain::list& new_blocks,
    const block_chain::list& replaced_blocks)
{
    const auto result = full_node::handle_new_blocks(ec, fork_point,
        new_blocks, replaced_blocks);

    if (ec == bc::error::service_stopped)
        return false;

    if (fork_point < last_checkpoint_height_)
        return false;

    // Fire server protocol block subscription notifications.
    for (auto new_block: new_blocks)
    {
        const size_t height = ++fork_point;
        for (const auto notify: block_sunscriptions_)
            notify(height, *new_block);
    }

    return result;
}

void server_node::fullnode_fetch_history(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    uint32_t from_height;
    payment_address address;

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
