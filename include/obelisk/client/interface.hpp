#ifndef OBELISK_CLIENT_INTERFACE
#define OBELISK_CLIENT_INTERFACE

#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/blockchain.hpp>
#include <obelisk/client/transaction_pool.hpp>

class subscriber_part
{
public:
    typedef std::function<void (size_t, const bc::block_type&)>
        block_notify_callback;
    typedef std::function<void (const bc::transaction_type&)>
        transaction_notify_callback;

    subscriber_part(zmq::context_t& context);

    // Non-copyable
    subscriber_part(const subscriber_part&) = delete;
    void operator=(const subscriber_part&) = delete;

    bool subscribe_blocks(const std::string& connection,
        block_notify_callback notify_block);
    bool subscribe_transactions(const std::string& connection,
        transaction_notify_callback notify_tx);
    void update();

private:
    typedef std::unique_ptr<zmq::socket_t> zmq_socket_uniqptr;

    bool setup_socket(const std::string& connection,
        zmq_socket_uniqptr& socket);

    void recv_tx();
    void recv_block();

    zmq::context_t& context_;
    zmq_socket_uniqptr socket_block_, socket_tx_;
    block_notify_callback notify_block_;
    transaction_notify_callback notify_tx_;
};

class fullnode_interface
{
public:
    fullnode_interface(const std::string& connection);

    // Non-copyable
    fullnode_interface(const fullnode_interface&) = delete;
    void operator=(const fullnode_interface&) = delete;

    bool subscribe_blocks(const std::string& connection,
        subscriber_part::block_notify_callback notify_block);
    bool subscribe_transactions(const std::string& connection,
        subscriber_part::transaction_notify_callback notify_tx);
    void update();
    void stop(const std::string& secret);
    blockchain_interface blockchain;
    transaction_pool_interface transaction_pool;

    void fetch_history(const bc::payment_address& address,
        bc::blockchain::fetch_handler_history handle_fetch,
        size_t from_height=0);

private:
    zmq::context_t context_;
    backend_cluster backend_;
    subscriber_part subscriber_;
};

#endif

