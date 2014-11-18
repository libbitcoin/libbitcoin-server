#ifndef OBELISK_WORKER_NODE_IMPL_HPP
#define OBELISK_WORKER_NODE_IMPL_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node.hpp>
#include "config.hpp"

namespace obelisk {

class node_impl
{
public:
    typedef std::function<void (size_t, const bc::block_type&)>
        block_notify_callback;
    typedef std::function<void (const bc::transaction_type&)>
        transaction_notify_callback;

    node_impl(config_type& config);
    bool start(config_type& config);
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
    void handle_mempool_store(
        const std::error_code& ec, const bc::index_list& unconfirmed,
        const bc::transaction_type& tx, bc::network::channel_ptr node);

    void reorganize(const std::error_code& ec,
        size_t fork_point,
        const bc::chain::blockchain::block_list& new_blocks,
        const bc::chain::blockchain::block_list& replaced_blocks);

    std::ofstream outfile_, errfile_;
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

} // namespace obelisk

#endif

