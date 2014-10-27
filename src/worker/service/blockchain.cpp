#include "../echo.hpp"
#include "../node_impl.hpp"
#include "blockchain.hpp"
#include "fetch_x.hpp"
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

void blockchain_fetch_transaction(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    hash_digest tx_hash;
    if (!unwrap_fetch_transaction_args(tx_hash, request))
        return;
    log_debug(LOG_REQUEST) << "blockchain.fetch_transaction("
        << tx_hash << ")";
    node.blockchain().fetch_transaction(tx_hash,
        std::bind(transaction_fetched, _1, _2, request, queue_send));
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
    const hash_list& hashes,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_block_transaction_hashes(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.size() == 32)
        fetch_block_transaction_hashes_by_hash(node, request, queue_send);
    //else if (data.size() == 4)
    //    fetch_block_transaction_hashes_by_height(node, request, queue_send);
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
    //node.blockchain().fetch_block_transaction_hashes(height,
    //    std::bind(block_transaction_hashes_fetched,
    //        _1, _2, request, queue_send));
}
void block_transaction_hashes_fetched(const std::error_code& ec,
    const hash_list& hashes,
    const incoming_message& request, queue_send_callback queue_send)
{
    data_chunk result(4 + hash_size * hashes.size());
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

void stealth_fetched(const std::error_code& ec,
    const blockchain::stealth_list& stealth_results,
    const incoming_message& request, queue_send_callback queue_send);
void blockchain_fetch_stealth(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& data = request.data();
    if (data.empty())
    {
        log_error(LOG_WORKER)
            << "Incorrect data size (empty) for blockchain.fetch_stealth";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    // number_bits
    uint8_t number_bits = deserial.read_byte();
    stealth_prefix prefix(number_bits);
    log_info(LOG_WORKER) << data.size() << " " << prefix.num_blocks();
    if (data.size() != 1 + prefix.num_blocks() + 4)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size (" << data.size()
	    << ") for blockchain.fetch_stealth";
        return;
    }
    // actual bitfield data
    data_chunk bitfield = deserial.read_data(prefix.num_blocks());
    boost::from_block_range(bitfield.begin(), bitfield.end(), prefix);
    // from_height
    size_t from_height = deserial.read_4_bytes();
    node.blockchain().fetch_stealth(prefix,
        std::bind(stealth_fetched, _1, _2, request, queue_send), from_height);
}
void stealth_fetched(const std::error_code& ec,
    const blockchain::stealth_list& stealth_results,
    const incoming_message& request, queue_send_callback queue_send)
{
    // [ ephemkey:33 ][ address:21 ][ tx_hash:32 ]
    constexpr size_t row_size = 33 + 21 + 32;
    data_chunk result(4 + row_size * stealth_results.size());
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    for (const blockchain::stealth_row row: stealth_results)
    {
        serial.write_data(row.ephemkey);
        serial.write_byte(row.address.version());
        serial.write_short_hash(row.address.hash());
        serial.write_hash(row.transaction_hash);
    }
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_stealth() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

} // namespace obelisk

