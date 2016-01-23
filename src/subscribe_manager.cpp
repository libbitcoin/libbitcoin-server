/**
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
#include <bitcoin/server/subscribe_manager.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

namespace posix_time = boost::posix_time;
using posix_time::second_clock;

static void register_with_node(subscribe_manager& manager, server_node& server)
{
    const auto receive_block = [&manager](size_t height, const block_type& block)
    {
        const auto block_hash = hash_block_header(block.header);
        for (const auto& tx: block.transactions)
            manager.submit(height, block_hash, tx);
    };

    const auto receive_tx = [&manager](const transaction_type& tx)
    {
        static constexpr size_t height = 0;
        manager.submit(height, null_hash, tx);
    };

    server.subscribe_blocks(receive_block);
    server.subscribe_transactions(receive_tx);
}

subscribe_manager::subscribe_manager(server_node& server,
    uint32_t maximum_subscriptions, uint32_t subscription_expiration_minutes)
  : strand_(server.pool()),
    subscription_limit_(maximum_subscriptions),
    subscription_expiration_minutes_(subscription_expiration_minutes)
{
    // subscribe to blocks and txs -> submit
    register_with_node(*this, server);
}

static subscribe_type convert_subscribe_type(uint8_t type_byte)
{
    return type_byte == 0 ? subscribe_type::address : subscribe_type::stealth;
}

// Private class typedef so use a template function.
template <typename AddressPrefix>
bool deserialize_address(AddressPrefix& address, subscribe_type& type,
    const data_chunk& data)
{
    auto deserial = make_deserializer(data.begin(), data.end());
    try
    {
        type = convert_subscribe_type(deserial.read_byte());
        auto bitsize = deserial.read_byte();
        auto blocks = deserial.read_data(binary_type::blocks_size(bitsize));
        address = AddressPrefix(bitsize, blocks);
    }
    catch (const end_of_stream&)
    {
        return false;
    }

    if (deserial.iterator() != data.end())
        return false;

    return true;
}

void subscribe_manager::subscribe(const incoming_message& request,
    queue_send_callback queue_send)
{
    strand_.queue(
        &subscribe_manager::do_subscribe,
            this, request, queue_send);
}
std::error_code subscribe_manager::add_subscription(
    const incoming_message& request, queue_send_callback queue_send)
{
    subscribe_type type;
    address_prefix address_key;
    if (!deserialize_address(address_key, type, request.data()))
    {
        log_warning(LOG_SUBSCRIBER) << "Incorrect format for subscribe data.";
        return error::bad_stream;
    }

    // Limit absolute number of subscriptions to prevent exhaustion attacks.
    if (subscriptions_.size() >= subscription_limit_)
        return error::pool_filled;

    // Now create subscription.
    const auto now = second_clock::universal_time();
    const auto expire_time = now + subscription_expiration_minutes_;
    const subscription new_subscription = 
    {
        address_key, 
        expire_time, 
        request.origin(), 
        queue_send, 
        type
    };

    subscriptions_.emplace_back(new_subscription);

    return std::error_code();
}
void subscribe_manager::do_subscribe(const incoming_message& request,
    queue_send_callback queue_send)
{
    const auto ec = add_subscription(request, queue_send);

    // Send response.
    data_chunk result(sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    outgoing_message response(request, result);
    queue_send(response);
}

void subscribe_manager::renew(const incoming_message& request,
    queue_send_callback queue_send)
{
    strand_.randomly_queue(
        &subscribe_manager::do_renew, this, request, queue_send);
}
void subscribe_manager::do_renew(const incoming_message& request,
    queue_send_callback queue_send)
{
    subscribe_type type;
    address_prefix address_key;
    if (!deserialize_address(address_key, type, request.data()))
    {
        log_warning(LOG_SUBSCRIBER) << "Incorrect format for subscribe renew.";
        return;
    }

    const auto now = second_clock::universal_time();
    const auto expire_time = now + subscription_expiration_minutes_;

    // Find entry and update expiry_time.
    for (auto& subscription: subscriptions_)
    {
        // Find matching subscription.
        if (subscription.prefix != address_key)
            continue;

        if (subscription.type != type)
            continue;

        // Only update subscriptions which were created by
        // the same client as this request originated from.
        if (subscription.client_origin != request.origin())
            continue;

        // Future expiry time.
        subscription.expiry_time = expire_time;
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
        &subscribe_manager::do_submit,
            this, height, block_hash, tx);
}

// TODO: move to collection utility.
template <typename Element>
std::vector<Element>& unique(std::vector<Element>& items)
{
    std::sort(items.begin(), items.end());
    items.erase(std::unique(items.begin(), items.end()), items.end());
    return items;
}

void subscribe_manager::do_submit(size_t height, const hash_digest& block_hash,
    const transaction_type& tx)
{
    payment_address address;
    std::vector<binary_type> prefixes;
    std::vector<payment_address> addresses;

    for (const auto& input: tx.inputs)
        if (extract(address, input.script))
            addresses.push_back(address);

    for (const auto& output: tx.outputs)
        if (extract(address, output.script))
            addresses.push_back(address);

    post(unique(addresses), height, block_hash, tx);

    // TODO: augment script::type with test for a corresponding spend output.
    for (const auto& output: tx.outputs)
        if (output.script.type() == payment_type::stealth_info)
            prefixes.push_back(calculate_stealth_prefix(output.script));

    post(unique(prefixes), height, block_hash, tx);

    // Periodicially sweep old expired entries (10 minute average).
    if (height > 0)
        sweep_expired();
}

void subscribe_manager::post(const std::vector<payment_address>& addresses,
    size_t height, const hash_digest& block_hash, const transaction_type& tx)
{
    for (const auto& address: addresses)
        post(addresses, height, block_hash, tx);
}

void subscribe_manager::post(const std::vector<binary_type>& prefixes,
    size_t height, const hash_digest& block_hash, const transaction_type& tx)
{
    for (const auto& prefix: prefixes)
        post(prefix, height, block_hash, tx);
}

void subscribe_manager::post(const payment_address& address, size_t height,
    const hash_digest& block_hash, const transaction_type& tx)
{
    BITCOIN_ASSERT(height <= max_uint32);
    const auto height32 = static_cast<uint32_t>(height);

    // [ addr.version ] (1 byte)
    // [ addr.hash ] (20 bytes)
    // [ height ] (4 bytes)
    // [ block_hash ] (32 bytes)
    // [ tx ]
    static constexpr size_t info_size = sizeof(uint8_t) + short_hash_size +
        sizeof(uint32_t) + hash_size;

    // TODO: defer this serialization until it's needed.
    const auto hash = address.hash();
    data_chunk data(info_size + satoshi_raw_size(tx));
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(hash);
    serial.write_4_bytes(height32);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);
    DEBUG_ONLY(auto rawtx_end_it =) satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(rawtx_end_it == data.end());

    // Send the result to everyone interested.
    for (const auto& subscription: subscriptions_)
    {
        if (subscription.type != subscribe_type::address)
            continue;

        binary_type match(subscription.prefix.size(), hash);
        if (match != subscription.prefix)
            continue;

        log_info(LOG_SERVICE)
            << "Subscribed address: " << address.encoded() << " found in tx ["
            << encode_hash(hash_transaction(tx)) << "]";

        const auto& origin = subscription.client_origin;
        outgoing_message update(origin, "address.update", data);
        subscription.queue_send(update);
    }
}

void subscribe_manager::post(const binary_type& prefix, size_t height,
    const hash_digest& block_hash, const transaction_type& tx)
{
    BITCOIN_ASSERT(height <= max_uint32);
    const auto height32 = static_cast<uint32_t>(height);

    // [ bitfield ] (4 bytes)
    // [ height ] (4 bytes)
    // [ block_hash ] (32 bytes)
    // [ tx ]
    static constexpr size_t info_size = sizeof(uint32_t) + sizeof(uint32_t) +
        hash_size;

    // TODO: defer this serialization until it's needed.
    data_chunk data(info_size + satoshi_raw_size(tx));
    auto serial = make_serializer(data.begin());
    serial.write_data(prefix.blocks());
    serial.write_4_bytes(height32);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);
    DEBUG_ONLY(auto rawtx_end_it =) satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(rawtx_end_it == data.end());

    // Send the result to everyone interested.
    for (const auto& subscription: subscriptions_)
    {
        if (subscription.type != subscribe_type::stealth)
            continue;

        binary_type match(subscription.prefix.size(), prefix.blocks());
        if (match != subscription.prefix)
            continue;

        log_info(LOG_SERVICE)
            << "Subscribed stealth prefix found in tx ["
            << encode_hash(hash_transaction(tx)) << "]";

        const auto& origin = subscription.client_origin;
        outgoing_message update(origin, "address.stealth_update", data);
        subscription.queue_send(update);
    }
}

void subscribe_manager::sweep_expired()
{
    const auto now = second_clock::universal_time();

    // Delete entries that have expired.
    for (auto it = subscriptions_.begin(); it != subscriptions_.end();)
    {
        const auto& subscription = *it;

        // Already expired? If so, then erase.
        if (subscription.expiry_time < now)
        {
            log_debug(LOG_SUBSCRIBER)
                << "Deleting expired subscription: "
                << subscription.prefix << " from "
                << encode_base16(subscription.client_origin);
            it = subscriptions_.erase(it);
            continue;
        }

        ++it;
    }
}

} // namespace server
} // namespace libbitcoin

