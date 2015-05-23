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
#ifndef LIBBITCOIN_SERVER_NODE_IMPL_HPP
#define LIBBITCOIN_SERVER_NODE_IMPL_HPP

#include <bitcoin/node.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/util.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node
  /* : full_node */
{
public:
    typedef std::function<void (size_t, const bc::block_type&)>
        block_notify_callback;
    typedef std::function<void (const bc::transaction_type&)>
        transaction_notify_callback;

    server_node(settings_type& config);
    bool start(settings_type& config);

    // Should only be called from the main thread.
    // It's an error to join() a thread from inside it.
    bool stop();

    void subscribe_blocks(block_notify_callback notify_block);
    void subscribe_transactions(transaction_notify_callback notify_tx);

    // Access to underlying services.
    bc::chain::blockchain& blockchain();
    bc::chain::transaction_pool& transaction_pool();
    bc::node::transaction_indexer& transaction_indexer();
    bc::network::protocol& protocol();

    // Threadpool for memory related operations.
    bc::threadpool& memory_related_threadpool();

    // TODO: use existing libbitcoin-node implementation.
    static void fullnode_fetch_history(server_node& node, 
        const incoming_message& request, queue_send_callback queue_send);

private:
    typedef std::vector<block_notify_callback> block_notify_list;
    typedef std::vector<transaction_notify_callback> transaction_notify_list;

    void start_session();
    void wait_and_retry_start(const std::error_code& ec);

    // New connection has been started.
    // Subscribe to new transaction messages from the network.
    void monitor_tx(const std::error_code& ec, bc::network::channel_ptr node);
    // New transaction message from the network.
    // Attempt to validate it by storing it in the transaction pool.
    void recv_transaction(const std::error_code& ec,
        const bc::transaction_type& tx, bc::network::channel_ptr node);
    // Result of store operation in transaction pool.
    void handle_mempool_store(const std::error_code& ec,
        const bc::index_list& unconfirmed, const bc::transaction_type& tx,
        bc::network::channel_ptr node);

    void reorganize(const std::error_code& ec, size_t fork_point,
        const bc::chain::blockchain::block_list& new_blocks,
        const bc::chain::blockchain::block_list& replaced_blocks);

    bc::ofstream outfile_, errfile_;
    // Threadpools
    bc::threadpool network_pool_, disk_pool_, mem_pool_;
    // Services
    bc::network::hosts hosts_;
    bc::network::handshake handshake_;
    bc::network::network network_;
    bc::network::protocol protocol_;
    bc::chain::blockchain_impl chain_;
    bc::node::poller poller_;
    bc::chain::transaction_pool txpool_;
    bc::node::transaction_indexer indexer_;
    bc::node::session session_;

    block_notify_list notify_blocks_;
    transaction_notify_list notify_txs_;

    boost::asio::deadline_timer retry_start_timer_;
};

} // namespace server
} // namespace libbitcoin


#endif

