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
#include <bitcoin/server/workers/address_worker.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <boost/date_time.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/services/query_service.hpp>
#include <bitcoin/server/utility/authenticator.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using namespace bc::chain;
using namespace bc::protocol;
using namespace bc::wallet;
using namespace boost::posix_time;

address_worker::address_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    secure_(secure),
    settings_(node.server_settings()),
    node_(node),
    authenticator_(authenticator)
{
}

// There is no unsubscribe so this class shouldn't be restarted.
bool address_worker::start()
{
    // TODO: connect subscriptions: work in progress.

    ////// Subscribe to blockchain reorganizations.
    ////node_.subscribe_blockchain(
    ////    std::bind(&block_service::handle_reorganization,
    ////        this, _1, _2, _3, _4));

    return zmq::worker::start();
}

// No unsubscribe so must be kept in scope until subscriber stop complete.
bool address_worker::stop()
{
    return zmq::worker::stop();
}

void address_worker::work()
{
    zmq::socket pair(authenticator_, zmq::socket::role::pair);

    // Connect socket to the worker endpoint.
    if (!started(connect(pair)))
        return;

    zmq::poller poller;
    poller.add(pair);

    // We will not receive on the poller, we use its timer and context stop.
    while (!poller.terminated() && !stopped())
    {
        poller.wait();
        prune();
    }

    // Disconnect the socket and exit this thread.
    finished(disconnect(pair));
}

// Connect/Disconnect.
//-----------------------------------------------------------------------------

bool address_worker::connect(socket& pair)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? query_service::secure_notify :
        query_service::public_notify;

    const auto ec = pair.connect(endpoint);

    if (ec)
    {
        log::error(LOG_SERVER)
            << "Failed to connect " << security << " address worker to "
            << endpoint << " : " << ec.message();
        return false;
    }

    log::debug(LOG_SERVER)
        << "Connected " << security << " address worker to " << endpoint;
    return true;
}

bool address_worker::disconnect(socket& pair)
{
    const auto security = secure_ ? "secure" : "public";

    // Don't log stop success.
    if (pair.stop())
        return true;

    log::error(LOG_SERVER)
        << "Failed to disconnect " << security << " address worker.";
    return false;
}

// Subscribe sequence.
// ----------------------------------------------------------------------------

void address_worker::subscribe(const incoming& request, send_handler handler)
{
    const auto ec = create(request, handler);

    // Send response.
    handler(outgoing(request, ec));
}

// Create new subscription entry.
code address_worker::create(const incoming& request, send_handler handler)
{
    subscription_record record;

    if (!deserialize(record.prefix, record.type, request.data))
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

void address_worker::renew(const incoming& request, send_handler handler)
{
    const auto ec = update(request, handler);

    // Send response.
    handler(outgoing(request, error::success));
}

// Find subscription record and update expiration.
code address_worker::update(const incoming& request, send_handler handler)
{
    binary prefix;
    subscribe_type type;

    if (!deserialize(prefix, type, request.data))
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
size_t address_worker::prune()
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

void address_worker::receive_block(uint32_t height, const block::ptr block)
{
    const auto hash = block->header.hash();

    for (const auto& transaction: block->transactions)
        scan(height, hash, transaction);

    prune();
}

void address_worker::receive_transaction(const transaction& transaction)
{
    scan(0, null_hash, transaction);
}

void address_worker::scan(uint32_t height, const hash_digest& block_hash,
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

void address_worker::post_updates(const payment_address& address,
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

void address_worker::post_stealth_updates(uint32_t prefix, uint32_t height,
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

ptime address_worker::now()
{
    return second_clock::universal_time();
};

bool address_worker::deserialize(binary& address, chain::subscribe_type& type,
    const data_chunk& data)
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
