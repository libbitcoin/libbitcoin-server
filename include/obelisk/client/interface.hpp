#ifndef OBELISK_CLIENT_INTERFACE
#define OBELISK_CLIENT_INTERFACE

#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/blockchain.hpp>
#include <obelisk/client/transaction_pool.hpp>
#include <obelisk/client/protocol.hpp>

namespace obelisk {

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

class address_subscriber
{
public:
    typedef std::function<void (
        const std::error_code&, size_t, const bc::hash_digest&,
        const bc::transaction_type&)> update_handler;

    typedef std::function<void (
        const std::error_code&, const worker_uuid&)> subscribe_handler;

    address_subscriber(bc::threadpool& pool, backend_cluster& backend);

    // Non-copyable
    address_subscriber(const address_subscriber&) = delete;
    void operator=(const address_subscriber&) = delete;

    // You should generally call subscribe() before fetch_history().
    void subscribe(const bc::payment_address& address,
        update_handler handle_update, subscribe_handler handle_subscribe);

    void fetch_history(const bc::payment_address& address,
        bc::blockchain::fetch_handler_history handle_fetch,
        size_t from_height=0, const worker_uuid& worker=worker_uuid());

    // unsubscribe() -> simply remove from subs_ map.

    // Sends renew messages to worker.
    void update();

private:
    struct subscription
    {
        const worker_uuid worker;
        update_handler handle_update;
    };

    typedef std::unordered_map<bc::payment_address, subscription>
        subscription_map;

    void receive_subscribe_result(
        const bc::data_chunk& data, const worker_uuid& worker,
        const bc::payment_address& address,
        update_handler handle_update, subscribe_handler handle_subscribe);
    void decode_reply(
        const bc::data_chunk& data, const worker_uuid& worker,
        subscribe_handler handle_subscribe);

    void receive_update(
        const bc::data_chunk& data, const worker_uuid& worker);
    void post_updates(
        const bc::payment_address& address, const worker_uuid& worker,
        size_t height, const bc::hash_digest& blk_hash,
        const bc::transaction_type& tx);

    backend_cluster& backend_;
    // Register subscription. Periodically send renew packets.
    bc::async_strand strand_;
    subscription_map subs_;
    // Send renew packets periodically.
    boost::posix_time::ptime last_renew_;
};

class fullnode_interface
{
public:
    fullnode_interface(bc::threadpool& pool, const std::string& connection);

    // Non-copyable
    fullnode_interface(const fullnode_interface&) = delete;
    void operator=(const fullnode_interface&) = delete;

    void update();

    bool subscribe_blocks(const std::string& connection,
        subscriber_part::block_notify_callback notify_block);
    bool subscribe_transactions(const std::string& connection,
        subscriber_part::transaction_notify_callback notify_tx);

private:
    zmq::context_t context_;
    backend_cluster backend_;
    subscriber_part subscriber_;

// These depend on the above components and
// should be constructed afterwards.
public:

    blockchain_interface blockchain;
    transaction_pool_interface transaction_pool;
    protocol_interface protocol;

    address_subscriber address;
};

} // namespace obelisk

#endif

