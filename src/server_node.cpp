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
#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/fetch_x.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::chain;
using namespace bc::node;
using namespace boost::posix_time;
using boost::format;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

const time_duration retry_start_duration = seconds(30);
constexpr auto append = std::ofstream::out | std::ofstream::app;

server_node::server_node(settings_type& config)
  : outfile_(config.debug_file.string(), append), 
    errfile_(config.error_file.string(), append),

    // Threadpools, the number of threads they spawn and priorities.
    network_pool_(1, thread_priority::normal),
    disk_pool_(6, thread_priority::low),
    mem_pool_(1, thread_priority::low),

    // Networking related services.
    hosts_(network_pool_, config.hosts_file.string(), 1000),
    handshake_(network_pool_),
    network_(network_pool_),
    protocol_(network_pool_, hosts_, handshake_, network_, 
        config.out_connections),

    // Blockchain database service.
    chain_(disk_pool_, config.blockchain_path.string(),
        { config.history_height }, 20),

    // Poll new blocks, tx memory pool and tx indexer.
    poller_(mem_pool_, chain_),
    txpool_(mem_pool_, chain_, config.tx_pool_capacity),
    indexer_(mem_pool_),

    // Session manager service.
    session_(mem_pool_, handshake_, protocol_, chain_, poller_, txpool_),
    retry_start_timer_(mem_pool_.service())
{
}

bool server_node::start(settings_type& config)
{
    // Set up logging for node background threads (add to config).
    const auto skip_log = if_else(config.log_requests, "", LOG_REQUEST);
    initialize_logging(outfile_, errfile_, bc::cout, bc::cerr, skip_log);

    // Start blockchain.
    if (!chain_.start())
    {
        log_error(LOG_NODE) << "Couldn't start blockchain.";
        return false;
    }

    chain_.subscribe_reorganize(
        std::bind(&server_node::reorganize, this, _1, _2, _3, _4));

    // Start transaction pool
    txpool_.start();

    // Outgoing connections setting in config file before we
    // start p2p network subsystem.
    protocol_.set_hosts_filename(config.hosts_file.string());
    if (!config.listener_enabled)
        protocol_.disable_listener();

    for (const auto& endpoint: config.peers)
    {
        const auto host = endpoint.get_host();
        const auto port = endpoint.get_port();
        log_info(LOG_NODE) << "Adding node: " << host << " " << port;
        protocol_.maintain_connection(host, port);
    }

    start_session();
    return true;
}
void server_node::start_session()
{
    // Start session
    const auto session_started = [this](const std::error_code& ec)
    {
        if (ec)
            wait_and_retry_start(ec);
    };
    session_.start(session_started);
}
void server_node::wait_and_retry_start(const std::error_code& ec)
{
    BITCOIN_ASSERT(ec);
    log_error(LOG_NODE) << "Unable to start session: " << ec.message();
    log_error(LOG_NODE) << "Retrying in "
        << retry_start_duration.seconds() << " seconds.";
    retry_start_timer_.expires_from_now(retry_start_duration);
    retry_start_timer_.async_wait(
        std::bind(&server_node::start_session, this));
}

bool server_node::stop()
{
    // Stop session
    std::promise<std::error_code> ec_session;
    const auto session_stopped = [&ec_session](const std::error_code& ec)
    {
        ec_session.set_value(ec);
    };
    session_.stop(session_stopped);
    // Query the error_code and wait for startup completion.
    const auto ec = ec_session.get_future().get();
    if (ec)
        log_error(LOG_NODE) << "Problem stopping session: " << ec.message();

    // Safely close blockchain database.
    chain_.stop();

    // Stop the threadpools.
    network_pool_.stop();
    disk_pool_.stop();
    mem_pool_.stop();
    // Join threadpools. Wait for them to finish.
    network_pool_.join();
    disk_pool_.join();
    mem_pool_.join();
    return true;
}

void server_node::subscribe_blocks(block_notify_callback notify_block)
{
    notify_blocks_.push_back(notify_block);
}
void server_node::subscribe_transactions(transaction_notify_callback notify_tx)
{
    notify_txs_.push_back(notify_tx);
}

blockchain& server_node::blockchain()
{
    return chain_;
}
transaction_pool& server_node::transaction_pool()
{
    return txpool_;
}
transaction_indexer& server_node::transaction_indexer()
{
    return indexer_;
}
network::protocol& server_node::protocol()
{
    return protocol_;
}
threadpool& server_node::memory_related_threadpool()
{
    return mem_pool_;
}

void server_node::monitor_tx(const std::error_code& ec, network::channel_ptr node)
{
    if (ec)
    {
        log_warning(LOG_NODE) << "Couldn't start connection: " << ec.message();
        return;
    }
    // Subscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&server_node::recv_transaction, this, _1, _2, node));
    // Stay subscribed to new connections.
    protocol_.subscribe_channel(
        std::bind(&server_node::monitor_tx, this, _1, _2));
}

void server_node::recv_transaction(const std::error_code& ec,
    const transaction_type& tx, network::channel_ptr node)
{
    if (ec)
    {
        log_warning(LOG_NODE) << "recv_transaction: " << ec.message();
        return;
    }
    const auto handle_deindex = [](const std::error_code& ec)
    {
        if (ec)
            log_error(LOG_NODE) << "Deindex error: " << ec.message();
    };
    // Called when the transaction becomes confirmed in a block.
    const auto handle_confirm = [this, tx, handle_deindex](
        const std::error_code& ec)
    {
        log_debug(LOG_NODE) << "Confirm transaction: " << ec.message()
            << " " << encode_hash(hash_transaction(tx));
        // Always try to deindex tx.
        // The error could be error::forced_removal from txpool.
        indexer_.deindex(tx, handle_deindex);
    };
    // Validate the transaction from the network.
    // Attempt to store in the transaction pool and check the result.
    txpool_.store(tx, handle_confirm,
        std::bind(&server_node::handle_mempool_store, this, _1, _2, tx, node));
    // Resubscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&server_node::recv_transaction, this, _1, _2, node));
}

void server_node::handle_mempool_store(
    const std::error_code& ec, const index_list& /* unconfirmed */,
    const transaction_type& tx, network::channel_ptr /* node */)
{
    if (ec)
    {
        log_warning(LOG_NODE) << "Failed to store transaction in mempool "
            << encode_hash(hash_transaction(tx)) << ": " << ec.message();
        return;
    }
    const auto handle_index = [](const std::error_code& ec)
    {
        if (ec)
            log_error(LOG_NODE) << "Index error: " << ec.message();
    };
    indexer_.index(tx, handle_index);
    log_info(LOG_NODE) << "Accepted transaction: "
        << encode_hash(hash_transaction(tx));
    for (const auto notify: notify_txs_)
        notify(tx);
}

void server_node::reorganize(const std::error_code& /* ec */,
    size_t fork_point,
    const blockchain::block_list& new_blocks,
    const blockchain::block_list& /* replaced_blocks */)
{
    // magic number (height) - how does this apply to testnet?
    // Don't bother publishing blocks when in the initial blockchain download.
    if (fork_point > 235866)
        for (size_t i = 0; i < new_blocks.size(); ++i)
        {
            size_t height = fork_point + i + 1;
            const block_type& blk = *new_blocks[i];
            for (const auto notify: notify_blocks_)
                notify(height, blk);
        }
    chain_.subscribe_reorganize(
        std::bind(&server_node::reorganize, this, _1, _2, _3, _4));
}

// TODO: use existing libbitcoin-node implementation.
void server_node::fullnode_fetch_history(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    uint32_t from_height;
    payment_address payaddr;
    if (!unwrap_fetch_history_args(payaddr, from_height, request))
        return;

    fetch_history(node.blockchain(), node.transaction_indexer(), payaddr,
        std::bind(send_history_result, _1, _2, request, queue_send),
        from_height);
}


} // namespace server
} // namespace libbitcoin
