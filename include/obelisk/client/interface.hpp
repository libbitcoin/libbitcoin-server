#ifndef OBELISK_CLIENT_INTERFACE
#define OBELISK_CLIENT_INTERFACE

#include <obelisk/client/blockchain.hpp>
#include <obelisk/client/transaction_pool.hpp>
#include <obelisk/client/protocol.hpp>
#include <obelisk/define.hpp>

namespace obelisk {

class subscriber_part
{
public:
    typedef std::function<void (size_t, const bc::block_type&)>
        block_notify_callback;
    typedef std::function<void (const bc::transaction_type&)>
        transaction_notify_callback;

    BCS_API subscriber_part(czmqpp::context& context);

    // Non-copyable
    BCS_API subscriber_part(const subscriber_part&) = delete;
    BCS_API void operator=(const subscriber_part&) = delete;

    BCS_API bool subscribe_blocks(const std::string& connection,
        block_notify_callback notify_block);
    BCS_API bool subscribe_transactions(const std::string& connection,
        transaction_notify_callback notify_tx);
    BCS_API void update();

private:
    bool setup_socket(
        const std::string& connection, czmqpp::socket& socket);

    void recv_tx();
    void recv_block();

    czmqpp::socket socket_block_, socket_tx_;
    block_notify_callback notify_block_;
    transaction_notify_callback notify_tx_;
};

typedef bc::stealth_prefix address_prefix;

class address_subscriber
{
public:
    typedef std::function<void (
        const std::error_code&, size_t, const bc::hash_digest&,
        const bc::transaction_type&)> update_handler;

    typedef std::function<void (
        const std::error_code&, const worker_uuid&)> subscribe_handler;

    BCS_API address_subscriber(bc::threadpool& pool, backend_cluster& backend);

    // Non-copyable
    BCS_API address_subscriber(const address_subscriber&) = delete;
    BCS_API void operator=(const address_subscriber&) = delete;

    // You should generally call subscribe() before fetch_history().
    BCS_API void subscribe(const address_prefix& prefix,
        update_handler handle_update, subscribe_handler handle_subscribe);

    BCS_API void fetch_history(const bc::payment_address& address,
        bc::blockchain::fetch_handler_history handle_fetch,
        size_t from_height=0, const worker_uuid& worker=worker_uuid());

    // unsubscribe() -> simply remove from subs_ map.

    // Sends renew messages to worker.
    void update();

private:
    struct subscription
    {
        address_prefix prefix;
        update_handler handle_update;
    };

    typedef std::vector<subscription> subscription_list;

    void receive_subscribe_result(
        const bc::data_chunk& data, const worker_uuid& worker,
        const address_prefix& prefix,
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
    subscription_list subs_;
    // Send renew packets periodically.
    boost::posix_time::ptime last_renew_;
};

class fullnode_interface
{
public:
    BCS_API fullnode_interface(bc::threadpool& pool, 
        const std::string& connection, const std::string& cert_filename="",
        const std::string& server_pubkey="");

    // Non-copyable
    BCS_API fullnode_interface(const fullnode_interface&) = delete;
    BCS_API void operator=(const fullnode_interface&) = delete;

    BCS_API void update();

    BCS_API bool subscribe_blocks(const std::string& connection,
        subscriber_part::block_notify_callback notify_block);
    BCS_API bool subscribe_transactions(const std::string& connection,
        subscriber_part::transaction_notify_callback notify_tx);

private:
    czmqpp::context context_;
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

