#include "blockchain.hpp"

#include "../node_impl.hpp"
#include "../echo.hpp"
#include "fetch_history.hpp"
#include "util.hpp"

namespace obelisk {

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

void chain_history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_history(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    payment_address payaddr;
    uint32_t from_height;
    if (!unwrap_fetch_history_args(payaddr, from_height, request))
        return;
    log_debug(LOG_REQUEST) << "blockchain.fetch_history("
        << payaddr.encoded() << ", from_height=" << from_height << ")";
    node.blockchain().fetch_history(payaddr,
        std::bind(send_history_result,
            _1, _2, request, queue_send), from_height);
}

void transaction_fetched(const std::error_code& ec,
    const transaction_type& tx,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_transaction(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() != 32)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_transaction";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest tx_hash = deserial.read_hash();
    node.blockchain().fetch_transaction(tx_hash,
        std::bind(transaction_fetched, _1, _2, request, queue_send));
}
void transaction_fetched(const std::error_code& ec,
    const transaction_type& tx,
    const incoming_message& request, queue_send_callback queue_send)
{
    data_chunk result(4 + satoshi_raw_size(tx));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    auto it = satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(it == result.end());
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_transaction() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

void last_height_fetched(const std::error_code& ec, size_t last_height,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_last_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (!data.empty())
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_last_height";
        return;
    }
    node.blockchain().fetch_last_height(
        std::bind(last_height_fetched, _1, _2, request, queue_send));
}
void last_height_fetched(const std::error_code& ec, size_t last_height,
    const incoming_message& request, queue_send_callback queue_send)
{
    data_chunk result(8);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    serial.write_4_bytes(last_height);
    BITCOIN_ASSERT(serial.iterator() == result.end());
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_last_height() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

void fetch_block_header_by_hash(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);
void fetch_block_header_by_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);
void block_header_fetched(const std::error_code& ec,
    const block_header_type& blk,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_block_header(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() == 32)
        fetch_block_header_by_hash(node, request, queue_send);
    else if (data.size() == 4)
        fetch_block_header_by_height(node, request, queue_send);
    else
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_block_header";
        return;
    }
}
void fetch_block_header_by_hash(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    BITCOIN_ASSERT(data.size() == 32);
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest blk_hash = deserial.read_hash();
    node.blockchain().fetch_block_header(blk_hash,
        std::bind(block_header_fetched, _1, _2, request, queue_send));
}
void fetch_block_header_by_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    BITCOIN_ASSERT(data.size() == 4);
    auto deserial = make_deserializer(data.begin(), data.end());
    size_t height = deserial.read_4_bytes();
    node.blockchain().fetch_block_header(height,
        std::bind(block_header_fetched, _1, _2, request, queue_send));
}
void block_header_fetched(const std::error_code& ec,
    const block_header_type& blk,
    const incoming_message& request, queue_send_callback queue_send)
{
    data_chunk result(4 + satoshi_raw_size(blk));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    auto it = satoshi_save(blk, serial.iterator());
    BITCOIN_ASSERT(it == result.end());
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_block_header() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

void fetch_block_transaction_hashes_by_hash(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);
void fetch_block_transaction_hashes_by_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);
void block_transaction_hashes_fetched(const std::error_code& ec,
    const hash_digest_list& hashes,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_block_transaction_hashes(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() == 32)
        fetch_block_transaction_hashes_by_hash(node, request, queue_send);
    else if (data.size() == 4)
        fetch_block_transaction_hashes_by_height(node, request, queue_send);
    else
    {
        log_error(LOG_WORKER) << "Incorrect data size for "
            "blockchain.fetch_block_transaction_hashes_by_height";
        return;
    }
}
void fetch_block_transaction_hashes_by_hash(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    BITCOIN_ASSERT(data.size() == 32);
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest blk_hash = deserial.read_hash();
    node.blockchain().fetch_block_transaction_hashes(blk_hash,
        std::bind(block_transaction_hashes_fetched,
            _1, _2, request, queue_send));
}
void fetch_block_transaction_hashes_by_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    BITCOIN_ASSERT(data.size() == 4);
    auto deserial = make_deserializer(data.begin(), data.end());
    size_t height = deserial.read_4_bytes();
    node.blockchain().fetch_block_transaction_hashes(height,
        std::bind(block_transaction_hashes_fetched,
            _1, _2, request, queue_send));
}
void block_transaction_hashes_fetched(const std::error_code& ec,
    const hash_digest_list& hashes,
    const incoming_message& request, queue_send_callback queue_send)
{
    data_chunk result(4 + hash_digest_size * hashes.size());
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    for (const hash_digest& tx_hash: hashes)
        serial.write_hash(tx_hash);
    BITCOIN_ASSERT(serial.iterator() == result.end());
    log_debug(LOG_REQUEST) << "blockchain.fetch_block_transaction_hashes()"
       " finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

void transaction_index_fetched(const std::error_code& ec,
    size_t block_height, size_t index,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_transaction_index(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() != 32)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_transaction_index";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest tx_hash = deserial.read_hash();
    node.blockchain().fetch_transaction_index(tx_hash,
        std::bind(transaction_index_fetched, _1, _2, _3, request, queue_send));
}
void transaction_index_fetched(const std::error_code& ec,
    size_t block_height, size_t index,
    const incoming_message& request, queue_send_callback queue_send)
{
    // error_code (4), block_height (4), index (4)
    data_chunk result(12);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    serial.write_4_bytes(block_height);
    serial.write_4_bytes(index);
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_transaction_index() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

void spend_fetched(const std::error_code& ec, const input_point& inpoint,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_spend(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() != 36)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_spend";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    output_point outpoint;
    outpoint.hash = deserial.read_hash();
    outpoint.index = deserial.read_4_bytes();
    node.blockchain().fetch_spend(outpoint,
        std::bind(spend_fetched, _1, _2, request, queue_send));
}
void spend_fetched(const std::error_code& ec, const input_point& inpoint,
    const incoming_message& request, queue_send_callback queue_send)
{
    // error_code (4), hash (32), index (4)
    data_chunk result(40);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    serial.write_hash(inpoint.hash);
    serial.write_4_bytes(inpoint.index);
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_spend() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

void block_height_fetched(const std::error_code& ec, size_t block_height,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_block_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() != 32)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_block_height";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest blk_hash = deserial.read_hash();
    node.blockchain().fetch_block_height(blk_hash,
        std::bind(block_height_fetched, _1, _2, request, queue_send));
}
void block_height_fetched(const std::error_code& ec, size_t block_height,
    const incoming_message& request, queue_send_callback queue_send)
{
    // error_code (4), height (4)
    data_chunk result(8);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    serial.write_4_bytes(block_height);
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_block_height() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

} // namespace obelisk

