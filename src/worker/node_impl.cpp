#include "node_impl.hpp"

#include <future>
#include <boost/lexical_cast.hpp>

namespace obelisk {

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

void log_to_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    if (body.empty())
        return;
    file << level_repr(level);
    if (!domain.empty())
        file << " [" << domain << "]";
    file << ": " << body << std::endl;
}
void log_to_both(std::ostream& device, std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    if (body.empty())
        return;
    std::ostringstream output;
    output << level_repr(level);
    if (!domain.empty())
        output << " [" << domain << "]";
    output << ": " << body;
    device << output.str() << std::endl;
    file << output.str() << std::endl;
}

void output_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_file(file, level, domain, body);
}
void output_both(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_both(std::cout, file, level, domain, body);
}

void error_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_file(file, level, domain, body);
}
void error_both(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body)
{
    log_to_both(std::cerr, file, level, domain, body);
}

node_impl::node_impl()
  : network_pool_(1), disk_pool_(6), mem_pool_(1),
    hosts_(network_pool_),
    handshake_(network_pool_),
    network_(network_pool_),
    protocol_(network_pool_, hosts_, handshake_, network_),
    chain_(disk_pool_),
    poller_(mem_pool_, chain_),
    txpool_(mem_pool_, chain_),
    indexer_(mem_pool_),
    session_(mem_pool_, {
        handshake_, protocol_, chain_, poller_, txpool_})
{
}

bool node_impl::start(config_map_type& config)
{
    auto file_mode = std::ofstream::out | std::ofstream::app;
    outfile_.open(config["output-file"], file_mode);
    errfile_.open(config["error-file"], file_mode);
    log_debug().set_output_function(
        std::bind(output_file, std::ref(outfile_), _1, _2, _3));
    log_info().set_output_function(
        std::bind(output_both, std::ref(outfile_), _1, _2, _3));
    log_warning().set_output_function(
        std::bind(error_file, std::ref(errfile_), _1, _2, _3));
    log_error().set_output_function(
        std::bind(error_both, std::ref(errfile_), _1, _2, _3));
    log_fatal().set_output_function(
        std::bind(error_both, std::ref(errfile_), _1, _2, _3));
    protocol_.subscribe_channel(
        std::bind(&node_impl::monitor_tx, this, _1, _2));
    // Start blockchain.
    std::promise<std::error_code> ec_chain;
    auto blockchain_started =
        [&](const std::error_code& ec)
    {
        ec_chain.set_value(ec);
    };
    chain_.start(config["blockchain-path"], blockchain_started);
    // Query the error_code and wait for startup completion.
    std::error_code ec = ec_chain.get_future().get();
    if (ec)
    {
        log_error() << "Couldn't start blockchain: " << ec.message();
        return false;
    }
    chain_.subscribe_reorganize(
        std::bind(&node_impl::reorganize, this, _1, _2, _3, _4));
    // Transaction pool
    txpool_.start();
    // Outgoing connections setting in config file before we
    // start p2p network subsystem.
    int outgoing_connections = boost::lexical_cast<int>(
        config["outgoing-connections"]);
    protocol_.set_max_outbound(outgoing_connections);
    // Start session
    std::promise<std::error_code> ec_session;
    auto session_started =
        [&](const std::error_code& ec)
    {
        ec_session.set_value(ec);
    };
    session_.start(session_started);
    // Query the error_code and wait for startup completion.
    ec = ec_session.get_future().get();
    if (ec)
    {
        log_error() << "Unable to start session: " << ec.message();
        return false;
    }
    return true;
}

bool node_impl::stop()
{
    // Stop session
    std::promise<std::error_code> ec_session;
    auto session_stopped =
        [&](const std::error_code& ec)
    {
        ec_session.set_value(ec);
    };
    session_.stop(session_stopped);
    // Query the error_code and wait for startup completion.
    std::error_code ec = ec_session.get_future().get();
    if (ec)
        log_error() << "Problem stopping session: " << ec.message();
    // Stop the threadpools.
    network_pool_.stop();
    disk_pool_.stop();
    mem_pool_.stop();
    network_pool_.join();
    disk_pool_.join();
    mem_pool_.join();
    chain_.stop();
    return true;
}

void node_impl::subscribe_blocks(block_notify_callback notify_block)
{
    notify_blocks_.push_back(notify_block);
}
void node_impl::subscribe_transactions(transaction_notify_callback notify_tx)
{
    notify_txs_.push_back(notify_tx);
}

blockchain& node_impl::blockchain()
{
    return chain_;
}
transaction_pool& node_impl::transaction_pool()
{
    return txpool_;
}
transaction_indexer& node_impl::transaction_indexer()
{
    return indexer_;
}
protocol& node_impl::protocol()
{
    return protocol_;
}
threadpool& node_impl::memory_related_threadpool()
{
    return mem_pool_;
}

void node_impl::monitor_tx(const std::error_code& ec, channel_ptr node)
{
    if (ec)
    {
        log_warning() << "Couldn't start connection: " << ec.message();
        return;
    }
    node->subscribe_transaction(
        std::bind(&node_impl::recv_transaction, this, _1, _2, node));
    protocol_.subscribe_channel(
        std::bind(&node_impl::monitor_tx, this, _1, _2));
}

void node_impl::recv_transaction(const std::error_code& ec,
    const transaction_type& tx, channel_ptr node)
{
    if (ec)
    {
        log_error() << "recv_transaction: " << ec.message();
        return;
    }
    auto handle_deindex = [](const std::error_code& ec)
    {
        if (ec)
            log_error() << "Deindex error: " << ec.message();
    };
    // Called when the transaction becomes confirmed in a block.
    auto handle_confirm = [this, tx, handle_deindex](
        const std::error_code& ec)
    {
        log_debug() << "Confirm transaction: " << ec.message()
            << " " << hash_transaction(tx);
        // Always try to deindex tx.
        // The error could be error::forced_removal from txpool.
        indexer_.deindex(tx, handle_deindex);
    };
    txpool_.store(tx, handle_confirm,
        std::bind(&node_impl::handle_mempool_store, this, _1, _2, tx, node));
    node->subscribe_transaction(
        std::bind(&node_impl::recv_transaction, this, _1, _2, node));
}

void node_impl::handle_mempool_store(
    const std::error_code& ec, const index_list& unconfirmed,
    const transaction_type& tx, channel_ptr node)
{
    if (ec)
    {
        log_warning()
            << "Error storing memory pool transaction "
            << hash_transaction(tx) << ": " << ec.message();
        return;
    }
    auto handle_index = [](const std::error_code& ec)
    {
        if (ec)
            log_error() << "Index error: " << ec.message();
    };
    indexer_.index(tx, handle_index);
    log_info() << "Accepted transaction: " << hash_transaction(tx);
    for (auto notify: notify_txs_)
        notify(tx);
}

void node_impl::reorganize(const std::error_code& ec,
    size_t fork_point,
    const bc::blockchain::block_list& new_blocks,
    const bc::blockchain::block_list& replaced_blocks)
{
    // Don't bother publishing blocks when in the initial blockchain download.
    if (fork_point > 235866)
        for (size_t i = 0; i < new_blocks.size(); ++i)
        {
            size_t height = fork_point + i + 1;
            const block_type& blk = *new_blocks[i];
            for (auto notify: notify_blocks_)
                notify(height, blk);
        }
    chain_.subscribe_reorganize(
        std::bind(&node_impl::reorganize, this, _1, _2, _3, _4));
}

} // namespace obelisk

