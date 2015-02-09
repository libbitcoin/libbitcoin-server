/*
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "service/util.hpp"
#include "subscribe_manager.hpp"

#define LOG_SUBSCRIBER "subscriber"

namespace libbitcoin {
namespace server {

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

subscribe_type read_subscribe_type(uint8_t type_byte)
{
    if (type_byte == 0)
        return subscribe_type::address;
    return subscribe_type::stealth;
}

// Private class typedef so use a template function.
template <typename AddressPrefix>
bool deserialize_address(AddressPrefix& addr, subscribe_type& type,
    const data_chunk& data)
{
    auto deserial = make_deserializer(data.begin(), data.end());
    try
    {
        type = read_subscribe_type(deserial.read_byte());
        uint8_t bitsize = deserial.read_byte();
        data_chunk blocks = deserial.read_data(
            binary_type::blocks_size(bitsize));
        addr = AddressPrefix(bitsize, blocks);
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
    subscribe_type type;
    if (!deserialize_address(addr_key, type, request.data()))
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
        addr_key, expire_time, request.origin(), queue_send, type});
    return std::error_code();
}
void subscribe_manager::do_subscribe(
    const incoming_message& request, queue_send_callback queue_send)
{
    std::error_code ec = add_subscription(request, queue_send);
    // Send response.
    data_chunk result(sizeof(uint32_t));
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
    subscribe_type type;
    if (!deserialize_address(addr_key, type, request.data()))
    {
        log_warning(LOG_SUBSCRIBER) << "Incorrect format for subscribe renew.";
        return;
    }
    const posix_time::ptime now = second_clock::universal_time();
    // Find entry and update expiry_time.
    for (subscription& sub: subs_)
    {
        // Find matching subscription.
        if (sub.prefix != addr_key)
            continue;
        if (sub.type != type)
            continue;
        // Only update subscriptions which were created by
        // the same client as this request originated from.
        if (sub.client_origin != request.origin())
            continue;
        // Future expiry time.
        sub.expiry_time = now + sub_expiry;
    }
    // Send response.
    data_chunk result(sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, std::error_code());
    outgoing_message response(request, result);
    queue_send(response);
}

void subscribe_manager::submit(
    size_t height, const hash_digest& block_hash,
    const transaction_type& tx)
{
    strand_.queue(
        &subscribe_manager::do_submit, this, height, block_hash, tx);
}
void subscribe_manager::do_submit(
    size_t height, const hash_digest& block_hash,
    const transaction_type& tx)
{
    for (const transaction_input_type& input: tx.inputs)
    {
        payment_address addr;
        if (extract(addr, input.script))
        {
            post_updates(addr, height, block_hash, tx);
            continue;
        }
    }
    for (const transaction_output_type& output: tx.outputs)
    {
        payment_address addr;
        if (extract(addr, output.script))
        {
            post_updates(addr, height, block_hash, tx);
            continue;
        }
        if (output.script.type() == payment_type::stealth_info)
        {
            binary_type prefix = calculate_stealth_prefix(output.script);
            post_stealth_updates(prefix, height, block_hash, tx);
            continue;
        }
    }
    // Periodicially sweep old expired entries.
    // Use the block 10 minute window as a periodic trigger.
    if (height)
        sweep_expired();
}

void subscribe_manager::post_updates(const payment_address& address,
    size_t height, const hash_digest& block_hash,
    const transaction_type& tx)
{
    BITCOIN_ASSERT(height <= max_uint32);
    auto height32 = static_cast<uint32_t>(height);

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
    serial.write_4_bytes(height32);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);
    // Now write the tx part.
    DEBUG_ONLY(auto rawtx_end_it =) satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(rawtx_end_it == data.end());
    // Send the result to everyone interested.
    for (subscription& sub: subs_)
    {
        // Only interested in address subscriptions.
        if (sub.type != subscribe_type::address)
            continue;
        binary_type match(sub.prefix.size(), address.hash());
        if (match != sub.prefix)
            continue;
        outgoing_message update(
            sub.client_origin, "address.update", data);
        sub.queue_send(update);
    }
}

void subscribe_manager::post_stealth_updates(const binary_type& prefix,
    size_t height, const hash_digest& block_hash,
    const transaction_type& tx)
{
    BITCOIN_ASSERT(height <= max_uint32);
    auto height32 = static_cast<uint32_t>(height);

    // [ bitfield ] (4 bytes)
    // [ height ] (4 bytes)
    // [ block_hash ] (32 bytes)
    // [ tx ]
    constexpr size_t info_size = 
        sizeof(uint32_t) + sizeof(uint32_t) + hash_size;
    data_chunk data(info_size + satoshi_raw_size(tx));
    auto serial = make_serializer(data.begin());
    serial.write_data(prefix.blocks());
    serial.write_4_bytes(height32);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);
    // Now write the tx part.
    DEBUG_ONLY(auto rawtx_end_it =) satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(rawtx_end_it == data.end());
    // Send the result to everyone interested.
    for (subscription& sub: subs_)
    {
        if (sub.type != subscribe_type::stealth)
            continue;
        binary_type match(sub.prefix.size(), prefix.blocks());
        if (match != sub.prefix)
            continue;
        outgoing_message update(
            sub.client_origin, "address.stealth_update", data);
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
                << sub.prefix << " from " << encode_base16(sub.client_origin);
            it = subs_.erase(it);
        }
        else
            ++it;
    }
}

} // namespace server
} // namespace libbitcoin

