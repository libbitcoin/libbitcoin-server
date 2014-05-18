#include "subscribe_manager.hpp"

#include "service/util.hpp"

#define LOG_SUBSCRIBER "subscriber"

namespace obelisk {

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;
namespace posix_time = boost::posix_time;
using posix_time::minutes;
using posix_time::second_clock;

const posix_time::time_duration sub_expiry = minutes(10);

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

template <typename AddressPrefix>
bool deserialize_address(AddressPrefix& addr, const data_chunk& data)
{
    auto deserial = make_deserializer(data.begin(), data.end());
    try
    {
        uint8_t number_bits = deserial.read_byte();
        addr.resize(number_bits);
        data_chunk bitfield = deserial.read_data(addr.num_blocks());
        boost::from_block_range(bitfield.begin(), bitfield.end(), addr);
    }
    catch (end_of_stream)
    {
        return false;
    }
    if (deserial.iterator() != data.end())
        return false;
    return true;
}

void subscribe_manager::subscribe(
    const incoming_message& request, queue_send_callback queue_send)
{
    strand_.queue(
        &subscribe_manager::do_subscribe, this, request, queue_send);
}
std::error_code subscribe_manager::add_subscription(
    const incoming_message& request, queue_send_callback queue_send)
{
    address_prefix addr_key;
    if (!deserialize_address(addr_key, request.data()))
    {
        log_warning(LOG_SUBSCRIBER) << "Incorrect format for subscribe data.";
        return error::bad_stream;
    }
    // Now create subscription.
    const auto now = second_clock::universal_time();
    const auto expire_time = now + sub_expiry;
    // Limit absolute number of subscriptions to prevent exhaustion attacks.
    if (subs_.size() >= subscribe_limit_)
        return error::pool_filled;
    subs_.emplace_back(subscription{
        addr_key, expire_time, request.origin(), queue_send});
    return std::error_code();
}
void subscribe_manager::do_subscribe(
    const incoming_message& request, queue_send_callback queue_send)
{
    std::error_code ec = add_subscription(request, queue_send);
    // Send response.
    data_chunk result(4);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    outgoing_message response(request, result);
    queue_send(response);
}

void subscribe_manager::renew(
    const incoming_message& request, queue_send_callback queue_send)
{
    strand_.randomly_queue(
        &subscribe_manager::do_renew, this, request, queue_send);
}
void subscribe_manager::do_renew(
    const incoming_message& request, queue_send_callback queue_send)
{
    address_prefix addr_key;
    if (!deserialize_address(addr_key, request.data()))
    {
        log_warning(LOG_SUBSCRIBER) << "Incorrect format for subscribe renew.";
        return;
    }
    const posix_time::ptime now = second_clock::universal_time();
    // Find entry and update expiry_time.
    for (subscription& sub: subs_)
    {
        // Find matching prefix subscription.
        if (sub.prefix != addr_key)
            continue;
        // Only update subscriptions which were created by
        // the same client as this request originated from.
        if (sub.client_origin != request.origin())
            continue;
        // Future expiry time.
        sub.expiry_time = now + sub_expiry;
    }
    // Send response.
    data_chunk result(4);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, std::error_code());
    outgoing_message response(request, result);
    queue_send(response);
}

void subscribe_manager::submit(
    size_t height, const bc::hash_digest& block_hash,
    const bc::transaction_type& tx)
{
    strand_.queue(
        &subscribe_manager::do_submit, this, height, block_hash, tx);
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
    // Periodicially sweep old expired entries.
    // Use the block 10 minute window as a periodic trigger.
    if (height)
        sweep_expired();
}

void subscribe_manager::post_updates(const payment_address& address,
    size_t height, const bc::hash_digest& block_hash,
    const bc::transaction_type& tx)
{
    // [ addr,version ] (1 byte)
    // [ addr.hash ] (20 bytes)
    // [ height ] (4 bytes)
    // [ block_hash ] (32 bytes)
    // [ tx ]
    constexpr size_t info_size = 1 + short_hash_size + 4 + hash_size;
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
    for (subscription& sub: subs_)
    {
        const short_hash& addr = address.hash();
        if (!stealth_match(sub.prefix, addr.data()))
            continue;
        outgoing_message update(
            sub.client_origin, "address.update", data);
        sub.queue_send(update);
    }
}

void subscribe_manager::sweep_expired()
{
    // Delete entries that have expired.
    const posix_time::ptime now = second_clock::universal_time();
    for (auto it = subs_.begin(); it != subs_.end(); )
    {
        const subscription& sub = *it;
        // Already expired? If so, then erase.
        if (sub.expiry_time < now)
        {
            log_debug(LOG_SUBSCRIBER) << "Deleting expired subscription: "
                << sub.prefix << " from " << sub.client_origin;
            it = subs_.erase(it);
        }
        else
            ++it;
    }
}

} // namespace obelisk

