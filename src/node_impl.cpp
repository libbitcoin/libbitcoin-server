#include <future>
#include <iostream>
#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include "echo.hpp"
#include "node_impl.hpp"

namespace obelisk {

using namespace bc;
using namespace bc::chain;
using namespace bc::node;
using namespace boost::posix_time;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

const time_duration retry_start_duration = seconds(30);

static std::string make_log_string(log_level level,
    const std::string& domain, const std::string& body, bool log_requests)
{
    if (body.empty())
        return "";
    if (!log_requests && domain == LOG_REQUEST)
        return "";
    std::ostringstream output;
    output << microsec_clock::local_time().time_of_day() << " ";
    output << level_repr(level);
    if (!domain.empty())
        output << " [" << domain << "]";
    output << ": " << body;
    output << std::endl;
    return output.str();
}

void log_to_file(std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body, bool log_requests)
{
    file << make_log_string(level, domain, body, log_requests);
}
void log_to_both(std::ostream& device, std::ofstream& file, log_level level,
    const std::string& domain, const std::string& body, bool log_requests)
{
    std::string output;
    output = make_log_string(level, domain, body, log_requests);
    device << output;
    file << output;
}

node_impl::node_impl(config_type& config)
    // Threadpools and the number of threads they spawn.
    // 6 threads spawned in total.
  : network_pool_(1), disk_pool_(6), mem_pool_(1),
    // Networking related services.
    hosts_(network_pool_),
    handshake_(network_pool_),
    network_(network_pool_),
    protocol_(network_pool_, hosts_, handshake_, network_),
    // Blockchain database service.
    chain_(disk_pool_, config.blockchain_path,
        {config.history_db_active_height}),
    // Poll new blocks, tx memory pool and tx indexer.
    poller_(mem_pool_, chain_),
    txpool_(mem_pool_, chain_),
    indexer_(mem_pool_),
    // Session manager service. Convenience wrapper.
    session_(mem_pool_, {
        handshake_, protocol_, chain_, poller_, txpool_}),
    retry_start_timer_(mem_pool_.service())
{
}

bool node_impl::start(config_type& config)
{
    auto file_mode = std::ofstream::out | std::ofstream::app;
    outfile_.open(config.output_file, file_mode);
    errfile_.open(config.error_file, file_mode);
    log_debug().set_output_function(
        std::bind(log_to_file, std::ref(outfile_),
            _1, _2, _3, config.log_requests));
    log_info().set_output_function(
        std::bind(log_to_both, std::ref(std::cout), std::ref(outfile_),
            _1, _2, _3, config.log_requests));
    log_warning().set_output_function(
        std::bind(log_to_file, std::ref(errfile_),
            _1, _2, _3, config.log_requests));
    log_error().set_output_function(
        std::bind(log_to_both, std::ref(std::cerr), std::ref(errfile_),
            _1, _2, _3, config.log_requests));
    log_fatal().set_output_function(
        std::bind(log_to_both, std::ref(std::cerr), std::ref(errfile_),
            _1, _2, _3, config.log_requests));
    // Subscribe to new connections.
    protocol_.subscribe_channel(
        std::bind(&node_impl::monitor_tx, this, _1, _2));
    // Start blockchain.
    if (!chain_.start())
    {
        log_error() << "Couldn't start blockchain.";
        return false;
    }
    chain_.subscribe_reorganize(
        std::bind(&node_impl::reorganize, this, _1, _2, _3, _4));
    // Start transaction pool
    txpool_.start();
    // Outgoing connections setting in config file before we
    // start p2p network subsystem.
    int outgoing_connections = boost::lexical_cast<int>(
        config.outgoing_connections);
    protocol_.set_max_outbound(outgoing_connections);
    protocol_.set_hosts_filename(config.hosts_file);
    if (!config.listener_enabled)
        protocol_.disable_listener();
    for (const auto node: config.nodes)
    {
        log_info() << "Adding node: " << node.hostname << " " << node.port;
        protocol_.maintain_connection(node.hostname, node.port);
    }
    start_session();
    return true;
}
void node_impl::start_session()
{
    // Start session
    auto session_started = [this](const std::error_code& ec)
    {
        if (ec)
            wait_and_retry_start(ec);
    };
    session_.start(session_started);
}
void node_impl::wait_and_retry_start(const std::error_code& ec)
{
    BITCOIN_ASSERT(ec);
    log_error() << "Unable to start session: " << ec.message();
    log_error() << "Retrying in "
        << retry_start_duration.seconds() << " seconds.";
    retry_start_timer_.expires_from_now(retry_start_duration);
    retry_start_timer_.async_wait(
        std::bind(&node_impl::start_session, this));
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
network::protocol& node_impl::protocol()
{
    return protocol_;
}
threadpool& node_impl::memory_related_threadpool()
{
    return mem_pool_;
}

void node_impl::monitor_tx(const std::error_code& ec, network::channel_ptr node)
{
    if (ec)
    {
        log_warning() << "Couldn't start connection: " << ec.message();
        return;
    }
    // Subscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&node_impl::recv_transaction, this, _1, _2, node));
    // Stay subscribed to new connections.
    protocol_.subscribe_channel(
        std::bind(&node_impl::monitor_tx, this, _1, _2));
}

void node_impl::recv_transaction(const std::error_code& ec,
    const transaction_type& tx, network::channel_ptr node)
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
    // Validate the transaction from the network.
    // Attempt to store in the transaction pool and check the result.
    txpool_.store(tx, handle_confirm,
        std::bind(&node_impl::handle_mempool_store, this, _1, _2, tx, node));
    // Resubscribe to transaction messages from this node.
    node->subscribe_transaction(
        std::bind(&node_impl::recv_transaction, this, _1, _2, node));
}

void node_impl::handle_mempool_store(
    const std::error_code& ec, const index_list& unconfirmed,
    const transaction_type& tx, network::channel_ptr node)
{
    if (ec)
    {
        log_warning()
            << "Failed to store transaction in mempool "
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
    const blockchain::block_list& new_blocks,
    const blockchain::block_list& replaced_blocks)
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

