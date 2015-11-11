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

#include <cstdint>
#include <boost/date_time.hpp>
#include <bitcoin/server/config/configuration.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::chain;
using namespace bc::wallet;

const auto now = []()
{
    return boost::posix_time::second_clock::universal_time();
};

static void register_with_node(subscribe_manager& manager, server_node& node)
{
    const auto receive_block = [&manager](size_t height, const block& block)
    {
        const auto block_hash = block.header.hash();

        for (const auto& tx: block.transactions)
            manager.submit(height, block_hash, tx);
    };

    const auto receive_tx = [&manager](const transaction& tx)
    {
        constexpr size_t height = 0;
        manager.submit(height, null_hash, tx);
    };

    node.subscribe_blocks(receive_block);
    node.subscribe_transactions(receive_tx);
}

subscribe_manager::subscribe_manager(server_node& node,
    const settings& settings)
  : dispatch_(node.pool()), settings_(settings)
{
    // subscribe to blocks and txs -> submit
    register_with_node(*this, node);
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

    return deserial.iterator() == data.end();
}

void subscribe_manager::subscribe(const incoming_message& request,
    queue_send_callback queue_send)
{
    dispatch_.ordered(
        &subscribe_manager::do_subscribe,
            this, request, queue_send);
}
code subscribe_manager::add_subscription(
    const incoming_message& request, queue_send_callback queue_send)
{
    subscribe_type type;
    binary_type address_key;
    if (!deserialize_address(address_key, type, request.data()))
    {
        log::warning(LOG_SUBSCRIBER)
            << "Incorrect format for subscribe data.";
        return error::bad_stream;
    }

    // Limit absolute number of subscriptions to prevent exhaustion attacks.
    if (subscriptions_.size() >= settings_.subscription_limit)
        return error::pool_filled;

    // Now create subscription.
    const auto expire_time = now() + settings_.subscription_expiration();
    const subscription new_subscription = 
    {
        address_key, 
        expire_time, 
        request.origin(), 
        queue_send, 
        type
    };

    subscriptions_.emplace_back(new_subscription);

    return code();
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
    dispatch_.unordered(
        &subscribe_manager::do_renew,
            this, request, queue_send);
}
void subscribe_manager::do_renew(const incoming_message& request,
    queue_send_callback queue_send)
{
    binary_type filter;
    subscribe_type type;
    if (!deserialize_address(filter, type, request.data()))
    {
        log::warning(LOG_SUBSCRIBER)
            << "Incorrect format for subscribe renew.";
        return;
    }

    const auto expire_time = now() + settings_.subscription_expiration();

    // Find entry and update expiry_time.
    for (auto& subscription: subscriptions_)
    {
        if (subscription.type != type)
            continue;

        // Only update subscriptions which were created by
        // the same client as this request originated from.
        if (subscription.client_origin != request.origin())
            continue;

        // Find matching subscription.
        if (!subscription.prefix.is_prefix_of(filter))
            continue;

        // Future expiry time.
        subscription.expiry_time = expire_time;
    }

    // Send response.
    data_chunk result(sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, code());
    outgoing_message response(request, result);
    queue_send(response);
}

void subscribe_manager::submit(size_t height, const hash_digest& block_hash,
    const transaction& tx)
{
    dispatch_.ordered(
        &subscribe_manager::do_submit,
            this, height, block_hash, tx);
}

void subscribe_manager::do_submit(size_t height, const hash_digest& block_hash,
    const transaction& tx)
{
    for (const auto& input: tx.inputs)
    {
        const auto address = payment_address::extract(input.script);
        if (address)
            post_updates(address, height, block_hash, tx);
    }

    uint32_t prefix;
    for (const auto& output: tx.outputs)
    {
        const auto address = payment_address::extract(output.script);
        if (address)
            post_updates(address, height, block_hash, tx);
        else if (to_stealth_prefix(prefix, output.script))
            post_stealth_updates(prefix, height, block_hash, tx);
    }

    // Periodicially sweep old expired entries.
    // Use the block 10 minute window as a periodic trigger.
    if (height)
        sweep_expired();
}

void subscribe_manager::post_updates(const payment_address& address,
    size_t height, const hash_digest& block_hash, const transaction& tx)
{
    BITCOIN_ASSERT(height <= max_uint32);
    const auto height32 = static_cast<uint32_t>(height);

    // [ address.version:1 ]
    // [ address.hash:20 ]
    // [ height:4 ]
    // [ block_hash:32 ]
    // [ tx ]
    static constexpr size_t info_size = 1 + short_hash_size + 4 + hash_size;

    data_chunk data(info_size + tx.serialized_size());
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    serial.write_4_bytes_little_endian(height32);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);

    // Now write the tx part.
    data_chunk tx_data = tx.to_data();
    serial.write_data(tx_data);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    // Send the result to everyone interested.
    for (const auto& subscription: subscriptions_)
    {
        if (subscription.type != subscribe_type::address)
            continue;

        if (!subscription.prefix.is_prefix_of(address.hash()))
            continue;

        outgoing_message update(subscription.client_origin, "address.update",
            data);

        subscription.queue_send(update);
    }
}

void subscribe_manager::post_stealth_updates(uint32_t prefix, size_t height, 
    const hash_digest& block_hash, const transaction& tx)
{
    BITCOIN_ASSERT(height <= max_uint32);
    const auto height32 = static_cast<uint32_t>(height);

    // [ prefix:4 ]
    // [ height:4 ] 
    // [ block_hash:32 ]
    // [ tx ]
    static constexpr size_t info_size = 2 * sizeof(uint32_t) + hash_size;

    data_chunk data(info_size + tx.serialized_size());
    auto serial = make_serializer(data.begin());
    serial.write_4_bytes_little_endian(prefix);
    serial.write_4_bytes_little_endian(height32);
    serial.write_hash(block_hash);
    BITCOIN_ASSERT(serial.iterator() == data.begin() + info_size);

    // Now write the tx part.
    data_chunk tx_data = tx.to_data();
    serial.write_data(tx_data);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    // Send the result to everyone interested.
    for (const auto& subscription: subscriptions_)
    {
        if (subscription.type != subscribe_type::stealth)
            continue;

            if (!subscription.prefix.is_prefix_of(prefix))
            continue;

        outgoing_message update(subscription.client_origin,
            "address.stealth_update", data);

        subscription.queue_send(update);
    }
}

void subscribe_manager::sweep_expired()
{
    const auto fixed_time = now();

    // Delete entries that have expired.
    for (auto it = subscriptions_.begin(); it != subscriptions_.end();)
    {
        const auto& subscription = *it;

        // Already expired? If so, then erase.
        if (subscription.expiry_time < fixed_time)
        {
            log::debug(LOG_SUBSCRIBER)
                << "Deleting expired subscription: " << subscription.prefix
                << " from " << encode_base16(subscription.client_origin);

            it = subscriptions_.erase(it);
            continue;
        }

        ++it;
    }
}

} // namespace server
} // namespace libbitcoin
