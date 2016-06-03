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
#include <bitcoin/server/utility/address_notifier.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <boost/date_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using namespace bc::chain;
using namespace bc::wallet;
using namespace boost::posix_time;

address_notifier::address_notifier(server_node& node)
  : node_(node),
    settings_(node.server_settings())
{
}

// Start.
// ----------------------------------------------------------------------------

// Subscribe against the node's tx and block publishers.
bool address_notifier::start()
{
    node_.subscribe_blocks(
        std::bind(&address_notifier::receive_block,
            this, _1, _2));

    node_.subscribe_transactions(
        std::bind(&address_notifier::receive_transaction,
            this, _1));

    return true;
}

// Subscribe sequence.
// ----------------------------------------------------------------------------

void address_notifier::subscribe(const incoming& request, send_handler handler)
{
    const auto ec = create(request, handler);

    // Send response.
    handler(outgoing(request, ec));
}

// Create new subscription entry.
code address_notifier::create(const incoming& request, send_handler handler)
{
    subscription_record record;

    if (!deserialize_address(record.prefix, record.type, request.data))
        return error::bad_stream;

    record.expiry_time = now() + settings_.subscription_expiration();
    record.locator.handler = std::move(handler);
    record.locator.address1 = request.address1;
    record.locator.address2 = request.address2;
    record.locator.delimited = request.delimited;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (subscriptions_.size() >= settings_.subscription_limit)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return error::pool_filled;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    subscriptions_.emplace_back(record);

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // This is the end of the subscribe sequence.
    return error::success;
}

// Renew sequence.
// ----------------------------------------------------------------------------

void address_notifier::renew(const incoming& request, send_handler handler)
{
    const auto ec = update(request, handler);

    // Send response.
    handler(outgoing(request, error::success));
}

// Find subscription record and update expiration.
code address_notifier::update(const incoming& request, send_handler handler)
{
    binary prefix;
    subscribe_type type;

    if (!deserialize_address(prefix, type, request.data))
        return error::bad_stream;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    const auto expiry_time = now() + settings_.subscription_expiration();

    for (auto& subscription: subscriptions_)
    {
        // TODO: is address1 correct and sufficient?
        if (subscription.type != type ||
            subscription.locator.address1 != request.address1 ||
            !subscription.prefix.is_prefix_of(prefix))
        {
            continue;
        }

        mutex_.unlock_upgrade_and_lock();
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        subscription.expiry_time = expiry_time;
        //---------------------------------------------------------------------
        mutex_.unlock_and_lock_upgrade();
    }

    mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    // This is the end of the renew sequence.
    return error::success;
}

// Pruning sequence.
// ----------------------------------------------------------------------------

// Delete entries that have expired.
size_t address_notifier::prune()
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    unique_lock(mutex_);

    const auto current_time = now();
    const auto start_size = subscriptions_.size();

    for (auto it = subscriptions_.begin(); it != subscriptions_.end();)
    {
        if (current_time < it->expiry_time)
            ++it;
        else
            it = subscriptions_.erase(it);
    }

    // This is the end of the pruning sequence.
    return start_size - subscriptions_.size();
    ///////////////////////////////////////////////////////////////////////////
}

// Scan sequence.
// ----------------------------------------------------------------------------

void address_notifier::receive_block(uint32_t height, const block::ptr block)
{
    const auto hash = block->header.hash();

    for (const auto& transaction: block->transactions)
        scan(height, hash, transaction);

    prune();
}

void address_notifier::receive_transaction(const transaction& transaction)
{
    scan(0, null_hash, transaction);
}

void address_notifier::scan(uint32_t height, const hash_digest& block_hash,
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
}

void address_notifier::post_updates(const payment_address& address,
    uint32_t height, const hash_digest& block_hash, const transaction& tx)
{
    subscription_locators locators;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_shared();

    for (const auto& subscription: subscriptions_)
        if (subscription.type == subscribe_type::address &&
            subscription.prefix.is_prefix_of(address.hash()))
                locators.push_back(subscription.locator);

    mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    if (locators.empty())
        return;

    // [ address.version:1 ]
    // [ address.hash:20 ]
    // [ height:4 ]
    // [ block_hash:32 ]
    // [ tx ]
    const auto data = build_chunk(
    {
        to_array(address.version()),
        address.hash(),
        to_little_endian(height),
        block_hash,
        tx.to_data()
    });

    // Send the result to everyone interested.
    for (const auto& locator: locators)
        locator.handler(outgoing("address.update", data, locator.address1,
                locator.address2, locator.delimited));

    // This is the end of the scan address sequence.
}

void address_notifier::post_stealth_updates(uint32_t prefix, uint32_t height,
    const hash_digest& block_hash, const transaction& tx)
{
    subscription_locators locators;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_shared();

    for (const auto& subscription: subscriptions_)
        if (subscription.type == subscribe_type::stealth &&
            subscription.prefix.is_prefix_of(prefix))
                locators.push_back(subscription.locator);

    mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    if (locators.empty())
        return;

    // [ prefix:4 ]
    // [ height:4 ] 
    // [ block_hash:32 ]
    // [ tx ]
    const auto data = build_chunk(
    {
        to_little_endian(prefix),
        to_little_endian(height),
        block_hash,
        tx.to_data()
    });

    // Send the result to everyone interested.
    for (const auto& locator: locators)
        locator.handler(outgoing("address.stealth_update", data,
            locator.address1, locator.address2, locator.delimited));

    // This is the end of the scan stealth sequence.
}

// Utilities
// ----------------------------------------------------------------------------

ptime address_notifier::now()
{
    return second_clock::universal_time();
};

bool address_notifier::deserialize_address(binary& address,
    chain::subscribe_type& type, const data_chunk& data)
{
    if (data.size() < 2)
        return false;

    // First byte is the subscribe_type enumeration.
    type = static_cast<chain::subscribe_type>(data[0]);

    // Second byte is the number of bits.
    const auto bit_length = data[1];

    // Convert the bit length to byte length.
    const auto byte_length = binary::blocks_size(bit_length);

    if (data.size() != byte_length + 2)
        return false;
    
    const data_chunk bytes({ data.begin() + 2, data.end() });
    address = binary(bit_length, bytes);
    return true;
}

} // namespace server
} // namespace libbitcoin
