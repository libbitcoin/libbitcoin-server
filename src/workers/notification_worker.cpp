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
#include <bitcoin/server/workers/notification_worker.hpp>

#include <cstdint>
#include <functional>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/services/query_service.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::protocol;
using namespace bc::wallet;

// TX RADAR
// By monitoring for tx inventory we do not poll and therefore will not
// suffer from peers that don't provide redundant responses, will not
// suffer double-counting, or have complexity and poor perf from caching.
// However this requires that the user subscribe *before* submitting a
// transaction. This feature is useful for the submitter of the tx to the
// network - generally the spender. This is intended to allow the spender
// to increase fees (ie using replace by fee). It is *not* recommended for
// use by a receiver (ie for the acceptance of zero confirmation txs).

notification_worker::notification_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    secure_(secure),
    settings_(node.server_settings()),
    node_(node),
    authenticator_(authenticator)
{
}

// There is no unsubscribe so this class shouldn't be restarted.
bool notification_worker::start()
{
    // Subscribe to blockchain reorganizations.
    node_.subscribe_blockchain(
        std::bind(&notification_worker::handle_blockchain_reorganization,
            this, _1, _2, _3, _4));

    // Subscribe to transaction pool acceptances.
    node_.subscribe_transaction_pool(
        std::bind(&notification_worker::handle_transaction_pool,
            this, _1, _2, _3));

    // Subscribe to all inventory messages from all peers.
    node_.subscribe<message::inventory>(
        std::bind(&notification_worker::handle_inventory,
            this, _1, _2));

    return zmq::worker::start();
}

// No unsubscribe so must be kept in scope until subscriber stop complete.
bool notification_worker::stop()
{
    return zmq::worker::stop();
}

// Implement worker as a router to the query service.
// The address worker receives no messages from the query service.
void notification_worker::work()
{
    zmq::socket router(authenticator_, zmq::socket::role::router);

    // Connect socket to the service endpoint.
    if (!started(connect(router)))
        return;

    zmq::poller poller;
    poller.add(router);

    // We do not send/receive on the poller, we use its timer and context stop.
    // Other threads connect and disconnect dynamically to send updates.
    while (!poller.terminated() && !stopped())
    {
        poller.wait();
        prune();
    }

    // Disconnect the socket and exit this thread.
    finished(disconnect(router));
}

// Connect/Disconnect.
//-----------------------------------------------------------------------------

bool notification_worker::connect(socket& router)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? query_service::secure_notify :
        query_service::public_notify;

    const auto ec = router.connect(endpoint);

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

bool notification_worker::disconnect(socket& router)
{
    const auto security = secure_ ? "secure" : "public";

    // Don't log stop success.
    if (router.stop())
        return true;

    log::error(LOG_SERVER)
        << "Failed to disconnect " << security << " address worker.";
    return false;
}

// Notification (via blockchain).
// ----------------------------------------------------------------------------

bool notification_worker::handle_blockchain_reorganization(const code& ec,
    uint64_t fork_point, const block_list& new_blocks, const block_list&)
{
    if (stopped() || ec == bc::error::service_stopped)
        return false;

    if (ec)
    {
        log::warning(LOG_SERVER)
            << "Failure handling new block: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    // Blockchain height is 64 bit but obelisk protocol is 32 bit.
    BITCOIN_ASSERT(fork_point <= max_uint32);
    const auto fork_point32 = static_cast<uint32_t>(fork_point);

    notify_blocks(fork_point32, new_blocks);
    return true;
}

void notification_worker::notify_blocks(uint32_t fork_point,
    const block_list& blocks)
{
    if (stopped())
        return;

    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? block_service::secure_worker :
        block_service::public_worker;

    // Notifications are off the pub-sub thread so this must connect back.
    // This could be optimized by caching the socket as thread static.
    zmq::socket publisher(authenticator_, zmq::socket::role::publisher);
    const auto ec = publisher.connect(endpoint);

    if (ec == bc::error::service_stopped)
        return;

    if (ec)
    {
        log::warning(LOG_SERVER)
            << "Failed to connect " << security << " block worker: "
            << ec.message();
        return;
    }

    BITCOIN_ASSERT(blocks.size() <= max_uint32);
    BITCOIN_ASSERT(fork_point < max_uint32 - blocks.size());
    auto height = fork_point;

    for (const auto block: blocks)
        notify_block(publisher, height++, block);
}

void notification_worker::notify_block(zmq::socket& publisher, uint32_t height,
    const block::ptr block)
{
    if (stopped())
        return;

    const auto block_hash = block->header.hash();

    for (const auto tx: block->transactions)
    {
        const auto tx_hash = tx.hash();

        notify_radar(height, block_hash, tx_hash);
        notify_transaction(height, block_hash, tx);
    }
}

// Notification (via transaction inventory).
// ----------------------------------------------------------------------------

bool notification_worker::handle_inventory(const code& ec,
    const message::inventory::ptr packet)
{
    if (stopped() || ec == bc::error::service_stopped)
        return false;

    if (ec)
    {
        log::warning(LOG_SERVER)
            << "Failure handling inventory: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    //*************************************************************************
    // TODO: loop inventories and extract transaction hashes.
    notify_radar(0, null_hash, packet->inventories.front().hash);
    //*************************************************************************

    return true;
}

// Notification (via mempool).
// ----------------------------------------------------------------------------

bool notification_worker::handle_transaction_pool(const code& ec,
    const point::indexes&, const transaction& tx)
{
    if (stopped() || ec == bc::error::service_stopped)
        return false;

    if (ec)
    {
        log::warning(LOG_SERVER)
            << "Failure handling new transaction: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    notify_transaction(0, null_hash, tx);
    return true;
}

void notification_worker::notify_transaction(uint32_t height,
    const hash_digest& block_hash, const transaction& tx)
{
    if (stopped())
        return;

    //*************************************************************************
    // TODO: loop outputs and extract payment addresses and stealth prefixes.
    uint32_t prefix = 42;
    payment_address address;

    while (false)
    {
        notify_address(address, height, block_hash, tx);
        notify_stealth(prefix, height, block_hash, tx);
    }
    //*************************************************************************
}

void notification_worker::notify_address(const payment_address& address,
    uint32_t height, const hash_digest& block_hash, const transaction& tx)
{
    // [ address.version:1 ]
    // [ address.hash:20 ]
    // [ height:4 ]
    // [ block_hash:32 ]
    // [ tx:... ]
    ////address_notifier_.relay(address, height, block_hash, tx);
}

void notification_worker::notify_stealth(uint32_t prefix, uint32_t height,
    const hash_digest& block_hash, const transaction& tx)
{
    // [ prefix:4 ]
    // [ height:4 ]
    // [ block_hash:32 ]
    // [ tx:... ]
    ////stealth_notifier_.relay(prefix, height, block_hash, tx);
}

void notification_worker::notify_radar(uint32_t height,
    const hash_digest& block_hash, const hash_digest& tx_hash)
{
    // Only provide height and hash if chained.
    // If chained or penetration is 100 (percent) drop subscription.
    // Only send messages at configured thresholds (e.g. 20/40/60/80/100%).
    // Thresholding allows the server to mask its peer count.
    // [ tx_hash:32 ]
    // [ penetration:1 ]
    // [ height:4 ]
    // [ block_hash:32 ]
    ////radar_notifier_.relay(height, block_hash, tx.hash());
}

// Subscribe sequence.
// ----------------------------------------------------------------------------

////void notification_worker::subscribe(const incoming& request, send_handler handler)
////{
////    const auto ec = create(request, handler);
////
////    // Send response.
////    handler(outgoing(request, ec));
////}
////
////// Create new subscription entry.
////code notification_worker::create(const incoming& request, send_handler handler)
////{
////    subscription_record record;
////
////    if (!deserialize(record.prefix, record.type, request.data))
////        return error::bad_stream;
////
////    record.expiry_time = now() + settings_.subscription_expiration();
////    record.locator.handler = std::move(handler);
////    record.locator.address1 = request.address1;
////    record.locator.address2 = request.address2;
////    record.locator.delimited = request.delimited;
////
////    ///////////////////////////////////////////////////////////////////////////
////    // Critical Section
////    mutex_.lock_upgrade();
////
////    if (subscriptions_.size() >= settings_.subscription_limit)
////    {
////        mutex_.unlock_upgrade();
////        //---------------------------------------------------------------------
////        return error::pool_filled;
////    }
////
////    mutex_.unlock_upgrade_and_lock();
////    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
////    subscriptions_.emplace_back(record);
////
////    mutex_.unlock();
////    ///////////////////////////////////////////////////////////////////////////
////
////    // This is the end of the subscribe sequence.
////    return error::success;
////}
////
////// Renew sequence.
////// ----------------------------------------------------------------------------
////
////void notification_worker::renew(const incoming& request, send_handler handler)
////{
////    const auto ec = update(request, handler);
////
////    // Send response.
////    handler(outgoing(request, error::success));
////}
////
////// Find subscription record and update expiration.
////code notification_worker::update(const incoming& request, send_handler handler)
////{
////    binary prefix;
////    subscribe_type type;
////
////    if (!deserialize(prefix, type, request.data))
////        return error::bad_stream;
////
////    ///////////////////////////////////////////////////////////////////////////
////    // Critical Section
////    mutex_.lock_upgrade();
////
////    const auto expiry_time = now() + settings_.subscription_expiration();
////
////    for (auto& subscription: subscriptions_)
////    {
////        // TODO: is address1 correct and sufficient?
////        if (subscription.type != type ||
////            subscription.locator.address1 != request.address1 ||
////            !subscription.prefix.is_prefix_of(prefix))
////        {
////            continue;
////        }
////
////        mutex_.unlock_upgrade_and_lock();
////        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
////        subscription.expiry_time = expiry_time;
////        //---------------------------------------------------------------------
////        mutex_.unlock_and_lock_upgrade();
////    }
////
////    mutex_.unlock_upgrade();
////    ///////////////////////////////////////////////////////////////////////////
////
////    // This is the end of the renew sequence.
////    return error::success;
////}
////
////// Pruning sequence.
////// ----------------------------------------------------------------------------
////
////// Delete entries that have expired.
////size_t notification_worker::prune()
////{
////    ///////////////////////////////////////////////////////////////////////////
////    // Critical Section
////    unique_lock(mutex_);
////
////    const auto current_time = now();
////    const auto start_size = subscriptions_.size();
////
////    for (auto it = subscriptions_.begin(); it != subscriptions_.end();)
////    {
////        if (current_time < it->expiry_time)
////            ++it;
////        else
////            it = subscriptions_.erase(it);
////    }
////
////    // This is the end of the pruning sequence.
////    return start_size - subscriptions_.size();
////    ///////////////////////////////////////////////////////////////////////////
////}
////
////// Scan sequence.
////// ----------------------------------------------------------------------------
////
////void notification_worker::receive_block(uint32_t height, const block::ptr block)
////{
////    const auto hash = block->header.hash();
////
////    for (const auto& transaction: block->transactions)
////        scan(height, hash, transaction);
////
////    prune();
////}
////
////void notification_worker::receive_transaction(const transaction& transaction)
////{
////    scan(0, null_hash, transaction);
////}
////
////void notification_worker::scan(uint32_t height, const hash_digest& block_hash,
////    const transaction& tx)
////{
////    for (const auto& input: tx.inputs)
////    {
////        const auto address = payment_address::extract(input.script);
////
////        if (address)
////            post_updates(address, height, block_hash, tx);
////    }
////
////    uint32_t prefix;
////    for (const auto& output: tx.outputs)
////    {
////        const auto address = payment_address::extract(output.script);
////
////        if (address)
////            post_updates(address, height, block_hash, tx);
////        else if (to_stealth_prefix(prefix, output.script))
////            post_stealth_updates(prefix, height, block_hash, tx);
////    }
////}
////
////void notification_worker::post_updates(const payment_address& address,
////    uint32_t height, const hash_digest& block_hash, const transaction& tx)
////{
////    subscription_locators locators;
////
////    ///////////////////////////////////////////////////////////////////////////
////    // Critical Section
////    mutex_.lock_shared();
////
////    for (const auto& subscription: subscriptions_)
////        if (subscription.type == subscribe_type::address &&
////            subscription.prefix.is_prefix_of(address.hash()))
////                locators.push_back(subscription.locator);
////
////    mutex_.unlock_shared();
////    ///////////////////////////////////////////////////////////////////////////
////
////    if (locators.empty())
////        return;
////
////    // [ address.version:1 ]
////    // [ address.hash:20 ]
////    // [ height:4 ]
////    // [ block_hash:32 ]
////    // [ tx ]
////    const auto data = build_chunk(
////    {
////        to_array(address.version()),
////        address.hash(),
////        to_little_endian(height),
////        block_hash,
////        tx.to_data()
////    });
////
////    // Send the result to everyone interested.
////    for (const auto& locator: locators)
////        locator.handler(outgoing("address.update", data, locator.address1,
////                locator.address2, locator.delimited));
////
////    // This is the end of the scan address sequence.
////}
////
////void notification_worker::post_stealth_updates(uint32_t prefix, uint32_t height,
////    const hash_digest& block_hash, const transaction& tx)
////{
////    subscription_locators locators;
////
////    ///////////////////////////////////////////////////////////////////////////
////    // Critical Section
////    mutex_.lock_shared();
////
////    for (const auto& subscription: subscriptions_)
////        if (subscription.type == subscribe_type::stealth &&
////            subscription.prefix.is_prefix_of(prefix))
////                locators.push_back(subscription.locator);
////
////    mutex_.unlock_shared();
////    ///////////////////////////////////////////////////////////////////////////
////
////    if (locators.empty())
////        return;
////
////    // [ prefix:4 ]
////    // [ height:4 ] 
////    // [ block_hash:32 ]
////    // [ tx ]
////    const auto data = build_chunk(
////    {
////        to_little_endian(prefix),
////        to_little_endian(height),
////        block_hash,
////        tx.to_data()
////    });
////
////    // Send the result to everyone interested.
////    for (const auto& locator: locators)
////        locator.handler(outgoing("address.stealth_update", data,
////            locator.address1, locator.address2, locator.delimited));
////
////    // This is the end of the scan stealth sequence.
////}
////
////// Utilities
////// ----------------------------------------------------------------------------
////
////ptime notification_worker::now()
////{
////    return second_clock::universal_time();
////};
////
////bool notification_worker::deserialize(binary& address, chain::subscribe_type& type,
////    const data_chunk& data)
////{
////    if (data.size() < 2)
////        return false;
////
////    // First byte is the subscribe_type enumeration.
////    type = static_cast<chain::subscribe_type>(data[0]);
////
////    // Second byte is the number of bits.
////    const auto bit_length = data[1];
////
////    // Convert the bit length to byte length.
////    const auto byte_length = binary::blocks_size(bit_length);
////
////    if (data.size() != byte_length + 2)
////        return false;
////    
////    const data_chunk bytes({ data.begin() + 2, data.end() });
////    address = binary(bit_length, bytes);
////    return true;
////}

} // namespace server
} // namespace libbitcoin
