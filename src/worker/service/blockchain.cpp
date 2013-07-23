#include "blockchain.hpp"

#include "../node_impl.hpp"
#include "../echo.hpp"

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;

template <typename Serializer>
void write_error_code(Serializer& serial, const std::error_code& ec)
{
    uint32_t val = ec.value();
    serial.write_4_bytes(val);
}

void history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    const incoming_message& request, zmq_socket_ptr socket);

void blockchain_fetch_history(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    if (data.size() != 1 + short_hash_size + 4)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_history";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    uint8_t version_byte = deserial.read_byte();
    short_hash hash = deserial.read_short_hash();
    uint32_t from_height = deserial.read_4_bytes();
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    payment_address payaddr;
    if (!payaddr.set_raw(version_byte, hash))
    {
        log_error(LOG_WORKER)
            << "Problem setting address in blockchain.fetch_history";
        return;
    }
    log_debug(LOG_WORKER) << "blockchain.fetch_history("
        << payaddr.encoded() << ")";
    node.blockchain().fetch_history(payaddr,
        std::bind(history_fetched, _1, _2, request, socket), from_height);
}
void history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    const incoming_message& request, zmq_socket_ptr socket)
{
    size_t row_size = 36 + 4 + 8 + 36 + 4;
    data_chunk result(4 + row_size * history.size());
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    for (const blockchain::history_row row: history)
    {
        serial.write_hash(row.output.hash);
        serial.write_4_bytes(row.output.index);
        serial.write_4_bytes(row.output_height);
        serial.write_8_bytes(row.value);
        serial.write_hash(row.spend.hash);
        serial.write_4_bytes(row.spend.index);
        serial.write_4_bytes(row.spend_height);
    }
    BITCOIN_ASSERT(serial.iterator() == result.end());
    outgoing_message response(request, result);
    log_debug(LOG_WORKER)
        << "blockchain.fetch_history() finished. Sending response.";
    response.send(*socket);
}

void transaction_fetched(const std::error_code& ec,
    const transaction_type& tx,
    const incoming_message& request, zmq_socket_ptr socket);
void blockchain_fetch_transaction(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    if (data.size() != 32)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_history";
        return;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest tx_hash = deserial.read_hash();
    node.blockchain().fetch_transaction(tx_hash,
        std::bind(transaction_fetched, _1, _2, request, socket));
}
void transaction_fetched(const std::error_code& ec,
    const transaction_type& tx,
    const incoming_message& request, zmq_socket_ptr socket)
{
    data_chunk result(4 + satoshi_raw_size(tx));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    auto it = satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(it == result.end());
    outgoing_message response(request, result);
    log_debug(LOG_WORKER)
        << "blockchain.fetch_transaction() finished. Sending response.";
    response.send(*socket);
}

void last_height_fetched(const std::error_code& ec, size_t last_height,
    const incoming_message& request, zmq_socket_ptr socket);
void blockchain_fetch_last_height(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    if (!data.empty())
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_last_height";
        return;
    }
    node.blockchain().fetch_last_height(
        std::bind(last_height_fetched, _1, _2, request, socket));
}
void last_height_fetched(const std::error_code& ec, size_t last_height,
    const incoming_message& request, zmq_socket_ptr socket)
{
    data_chunk result(8);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    serial.write_4_bytes(last_height);
    BITCOIN_ASSERT(serial.iterator() == result.end());
    outgoing_message response(request, result);
    log_debug(LOG_WORKER)
        << "blockchain.fetch_last_height() finished. Sending response.";
    response.send(*socket);
}

void fetch_block_header_by_hash(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);
void fetch_block_header_by_height(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);
void block_header_fetched(const std::error_code& ec,
    const block_header_type& blk,
    const incoming_message& request, zmq_socket_ptr socket);
void blockchain_fetch_block_header(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    if (data.size() == 32)
        fetch_block_header_by_hash(node, request, socket);
    else if (data.size() == 4)
        fetch_block_header_by_height(node, request, socket);
    else
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_block_header";
        return;
    }
}
void fetch_block_header_by_hash(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    BITCOIN_ASSERT(data.size() == 32);
    auto deserial = make_deserializer(data.begin(), data.end());
    const hash_digest blk_hash = deserial.read_hash();
    node.blockchain().fetch_block_header(blk_hash,
        std::bind(block_header_fetched, _1, _2, request, socket));
}
void fetch_block_header_by_height(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    BITCOIN_ASSERT(data.size() == 4);
    auto deserial = make_deserializer(data.begin(), data.end());
    size_t height = deserial.read_4_bytes();
    node.blockchain().fetch_block_header(height,
        std::bind(block_header_fetched, _1, _2, request, socket));
}
void block_header_fetched(const std::error_code& ec,
    const block_header_type& blk,
    const incoming_message& request, zmq_socket_ptr socket)
{
    data_chunk result(4 + satoshi_raw_size(blk));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    auto it = satoshi_save(blk, serial.iterator());
    BITCOIN_ASSERT(it == result.end());
    outgoing_message response(request, result);
    log_debug(LOG_WORKER)
        << "blockchain.fetch_block_header() finished. Sending response.";
    response.send(*socket);
}

