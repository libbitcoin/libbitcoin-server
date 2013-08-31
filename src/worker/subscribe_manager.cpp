#include "subscribe_manager.hpp"

#include "service/util.hpp"

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;
namespace posix_time = boost::posix_time;
using posix_time::minutes;
using posix_time::second_clock;

const posix_time::time_duration sub_expiry = minutes(20);

void register_with_node(subscribe_manager& manager, node_impl& node)
{
    auto recv_blk = [&manager](size_t height, const block_type& blk)
    {
        const hash_digest blk_hash = hash_block_header(blk.header);
        for (const transaction_type& tx: blk.transactions)
            manager.submit(height, blk_hash, tx);
    };
    auto recv_tx = [&manager](const transaction_type& tx)
    {
        manager.submit(0, null_hash, tx);
    };
    node.subscribe_blocks(recv_blk);
    node.subscribe_transactions(recv_tx);
}

subscribe_manager::subscribe_manager(node_impl& node)
  : strand_(node.memory_related_threadpool())
{
    // subscribe to blocks and txs -> submit
    register_with_node(*this, node);
}

void subscribe_manager::subscribe(
    const incoming_message& request, zmq_socket_ptr socket)
{
    strand_.queue(std::bind(&subscribe_manager::do_subscribe,
        this, request, socket));
}
void subscribe_manager::do_subscribe(
    const incoming_message& request, zmq_socket_ptr socket)
{
    // Read address.
    payment_address addr_key;
    const data_chunk& data = request.data();
    auto deserial = make_deserializer(data.begin(), data.end());
    uint8_t version_byte = deserial.read_byte();
    short_hash hash = deserial.read_short_hash();
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    addr_key.set(version_byte, hash);
    // Now create subscription.
    const posix_time::ptime now = second_clock::universal_time();
    subs_.emplace(addr_key, subscription{
        now + sub_expiry, request.origin(), socket});
    // Send response.
    data_chunk result(4);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, std::error_code());
    outgoing_message response(request, result);
    response.send(*socket);
}

void subscribe_manager::submit(
    size_t height, const bc::hash_digest& block_hash,
    const bc::transaction_type& tx)
{
    strand_.queue(std::bind(&subscribe_manager::do_submit,
        this, height, block_hash, tx));
}
void subscribe_manager::do_submit(
    size_t height, const bc::hash_digest& block_hash,
    const bc::transaction_type& tx)
{
    for (const transaction_input_type& input: tx.inputs)
    {
        payment_address addr;
        if (!extract(addr, input.script))
            continue;
        post_updates(addr, height, block_hash, tx);
    }
    for (const transaction_output_type& output: tx.outputs)
    {
        payment_address addr;
        if (!extract(addr, output.script))
            continue;
        post_updates(addr, height, block_hash, tx);
    }
}
void subscribe_manager::post_updates(const payment_address& address,
    size_t height, const bc::hash_digest& block_hash,
    const bc::transaction_type& tx)
{
    auto range = subs_.equal_range(address);
    // Avoid expensive serialization if not needed.
    if (range.first == range.second)
        return;
    // [ addr,version ] (1 byte)
    // [ addr.hash ] (20 bytes)
    // [ height ] (4 bytes)
    // [ block_hash ] (32 bytes)
    // [ tx ]
    constexpr size_t info_size = 1 + short_hash_size + 4 + hash_digest_size;
    data_chunk data(info_size + satoshi_raw_size(tx));
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    serial.write_4_bytes(height);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);
    // Now write the tx part.
    auto rawtx_end_it = satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(rawtx_end_it == data.end());
    // Send the result to everyone interested.
    for (auto it = range.first; it != range.second; ++it)
    {
        const subscription& sub_detail = it->second;
        outgoing_message update(
            sub_detail.client_origin, "address.update", data);
        update.send(*sub_detail.socket);
    }
}

