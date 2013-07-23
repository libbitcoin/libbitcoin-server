#include <obelisk/client/interface.hpp>

#include <bitcoin/bitcoin.hpp>
#include <obelisk/zmq_message.hpp>

using namespace bc;

#define LOG_SUBSCRIBER "subscriber"

subscriber_part::subscriber_part(zmq::context_t& context)
  : context_(context)
{
}

bool subscriber_part::setup_socket(const std::string& connection,
    zmq_socket_uniqptr& socket)
{
    socket.reset(new zmq::socket_t(context_, ZMQ_SUB));
    try
    {
        socket->connect(connection.c_str());
        socket->setsockopt(ZMQ_SUBSCRIBE, 0, 0);
    }
    catch (zmq::error_t error)
    {
        socket.reset();
        log_warning(LOG_SUBSCRIBER)
            << "Subscriber failed to connect: " << connection;
        BITCOIN_ASSERT(error.num() != 0);
        return false;
    }
    return true;
}

bool subscriber_part::subscribe_blocks(const std::string& connection,
    block_notify_callback notify_block)
{
    if (!setup_socket(connection, socket_block_))
        return false;
    notify_block_ = notify_block;
    return true;
}

bool subscriber_part::subscribe_transactions(const std::string& connection,
    transaction_notify_callback notify_tx)
{
    if (!setup_socket(connection, socket_tx_))
        return false;
    notify_tx_ = notify_tx;
    return true;
}

void subscriber_part::update()
{
    //  Poll socket for a reply, with timeout
    if (socket_tx_)
    {
        zmq::pollitem_t items[] = { { *socket_tx_, 0, ZMQ_POLLIN, 0 } };
        zmq::poll(&items[0], 1, 0);
        if (items[0].revents & ZMQ_POLLIN)
            recv_tx();
    }
    if (socket_block_)
    {
        zmq::pollitem_t items[] = { { *socket_block_, 0, ZMQ_POLLIN, 0 } };
        zmq::poll(&items[0], 1, 0);
        if (items[1].revents & ZMQ_POLLIN)
            recv_block();
    }
}

bool read_hash(hash_digest& hash, const data_chunk& raw_hash)
{
    if (raw_hash.size() != hash_digest_size)
    {
        log_warning(LOG_SUBSCRIBER) << "Wrong size for hash. Dropping.";
        return false;
    }
    auto deserial = make_deserializer(raw_hash.begin(), raw_hash.end());
    hash = deserial.read_hash();
    BITCOIN_ASSERT(deserial.iterator() == raw_hash.end());
    return true;
}

void subscriber_part::recv_tx()
{
    zmq_message message;
    bool success = message.recv(*socket_tx_);
    BITCOIN_ASSERT(success);
    // [ tx hash ]
    // [ raw tx ]
    const data_stack& parts = message.parts();
    if (parts.size() != 2)
    {
        log_warning(LOG_SUBSCRIBER) << "Malformed tx response. Dropping.";
        return;
    }
    hash_digest tx_hash;
    if (!read_hash(tx_hash, parts[0]))
        return;
    const data_chunk& raw_tx = parts[1];
    transaction_type tx;
    satoshi_load(raw_tx.begin(), raw_tx.end(), tx);
    if (hash_transaction(tx) != tx_hash)
    {
        log_warning(LOG_SUBSCRIBER)
            << "Tx hash and actual tx unmatched. Dropping.";
        return;
    }
    // Everything OK!
    notify_tx_(tx);
}

void subscriber_part::recv_block()
{
    zmq_message message;
    bool success = message.recv(*socket_block_);
    BITCOIN_ASSERT(success);
    // [ block hash ]
    // [ height ]
    // [ block data ]
    const data_stack& parts = message.parts();
    if (parts.size() != 3)
    {
        log_warning() << "Malformed block response. Dropping.";
        return;
    }
    hash_digest blk_hash;
    if (!read_hash(blk_hash, parts[0]))
        return;
    uint32_t height = cast_chunk<uint32_t>(parts[1]);
    const data_chunk& raw_blk = parts[2];
    block_type blk;
    satoshi_load(raw_blk.begin(), raw_blk.end(), blk);
    if (hash_block_header(blk.header) != blk_hash)
    {
        log_warning(LOG_SUBSCRIBER)
            << "Block hash and actual block unmatched. Dropping.";
        return;
    }
    // Everything OK!
    notify_block_(height, blk);
}

fullnode_interface::fullnode_interface(const std::string& connection)
  : context_(1), backend_(context_, connection), blockchain(backend_),
    subscriber_(context_)
{
}

bool fullnode_interface::subscribe_blocks(const std::string& connection,
    subscriber_part::block_notify_callback notify_block)
{
    return subscriber_.subscribe_blocks(connection, notify_block);
}
bool fullnode_interface::subscribe_transactions(const std::string& connection,
    subscriber_part::transaction_notify_callback notify_tx)
{
    return subscriber_.subscribe_transactions(connection, notify_tx);
}

void fullnode_interface::update()
{
    backend_.update();
    subscriber_.update();
}

void fullnode_interface::stop(const std::string& secret)
{
    bc::data_chunk data(secret.begin(), secret.end());
    outgoing_message message("stop", data);
    backend_.send(message);
}

