/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/workers/notification_worker.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/messages/route.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/services/query_service.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

#define NAME "notification_worker"

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::protocol;
using namespace bc::wallet;

// Purge subscriptions at 10% of the expiration period.
static constexpr int64_t purge_interval_ratio = 10;

// Notifications respond with commands that are distinct from the subscription.
////static const std::string penetration_update("penetration.update");
////static const std::string address_stealth("address.stealth_update");
////static const std::string address_update("address.update");
static const std::string address_update2("address.update2");

notification_worker::notification_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    secure_(secure),
    settings_(node.server_settings()),
    node_(node),
    authenticator_(authenticator),
    address_subscriber_(std::make_shared<address_subscriber>(
        node.thread_pool(), settings_.subscription_limit, NAME "_address"))
    ////penetration_subscriber_(std::make_shared<penetration_subscriber>(
    ////    node.thread_pool(), settings_.subscription_limit, NAME "_penetration"))
{
}

// There is no unsubscribe so this class shouldn't be restarted.
bool notification_worker::start()
{
    // v3
    address_subscriber_->start();
    ////penetration_subscriber_->start();

    // Subscribe to blockchain reorganizations.
    node_.subscribe_blockchain(
        std::bind(&notification_worker::handle_reorganization,
            this, _1, _2, _3, _4));

    // Subscribe to transaction pool acceptances.
    node_.subscribe_transaction(
        std::bind(&notification_worker::handle_transaction_pool,
            this, _1, _2));

    ////// BUGBUG: this API was removed as could not adapt to changing peers.
    ////// Subscribe to all inventory messages from all peers.
    ////node_.subscribe<bc::message::inventory>(
    ////    std::bind(&notification_worker::handle_inventories,
    ////        this, _1, _2));

    return zmq::worker::start();
}

// No unsubscribe so must be kept in scope until subscriber stop complete.
bool notification_worker::stop()
{
    static const auto code = error::channel_stopped;

    // v3
    address_subscriber_->stop();
    address_subscriber_->invoke(code, {}, 0, {}, {});

    ////penetration_subscriber_->stop();
    ////penetration_subscriber_->invoke(code, 0, {}, {});

    return zmq::worker::stop();
}

// Implement worker as a router to the query service.
// The notification worker receives no messages from the query service.
void notification_worker::work()
{
    zmq::socket router(authenticator_, zmq::socket::role::router);

    // Connect socket to the service endpoint.
    if (!started(connect(router)))
        return;

    zmq::poller poller;
    poller.add(router);
    const auto interval = purge_interval_milliseconds();

    // We do not send/receive on the poller, we use its timer and context stop.
    // Other threads connect and disconnect dynamically to send updates.
    while (!poller.terminated() && !stopped())
    {
        // BUGBUG: this can fail on some platforms if interval is > 1000.
        poller.wait(interval);
        purge();
    }

    // Disconnect the socket and exit this thread.
    finished(disconnect(router));
}

int32_t notification_worker::purge_interval_milliseconds() const
{
    const int64_t minutes = settings_.subscription_expiration_minutes;
    const int64_t milliseconds = minutes * 60 * 1000 / purge_interval_ratio;
    const auto capped = std::min(milliseconds, static_cast<int64_t>(max_int32));
    return static_cast<int32_t>(capped);
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
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect " << security << " notification worker to "
            << endpoint << " : " << ec.message();
        return false;
    }

    LOG_INFO(LOG_SERVER)
        << "Connected " << security << " notification worker to " << endpoint;
    return true;
}

bool notification_worker::disconnect(socket& router)
{
    const auto security = secure_ ? "secure" : "public";

    // Don't log stop success.
    if (router.stop())
        return true;

    LOG_ERROR(LOG_SERVER)
        << "Failed to disconnect " << security << " notification worker.";
    return false;
}

// Pruning.
// ----------------------------------------------------------------------------

// Signal expired subscriptions to self-remove.
void notification_worker::purge()
{
    static const auto code = error::channel_timeout;

    // v3
    address_subscriber_->purge(code, {}, 0, {}, {});
    ////penetration_subscriber_->purge(code, 0, {}, {});
}

// Sending.
// ----------------------------------------------------------------------------

void notification_worker::send(const route& reply_to,
    const std::string& command, uint32_t id, const data_chunk& payload)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? query_service::secure_notify :
        query_service::public_notify;

    zmq::socket notifier(authenticator_, zmq::socket::role::router);
    auto ec = notifier.connect(endpoint);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failed to connect " << security << " notification worker: "
            << ec.message();
        return;
    }

    // Notifications are formatted as query response messages.
    message notification(reply_to, command, id, payload);
    ec = notification.send(notifier);

    if (ec && ec != error::service_stopped)
        LOG_WARNING(LOG_SERVER)
            << "Failed to send notification to "
            << notification.route().display() << " " << ec.message();
}

void notification_worker::send_address(const route& reply_to, uint32_t id,
    uint8_t sequence, uint32_t height, const hash_digest& block_hash,
    transaction_const_ptr tx)
{
    // [ code:4 ]
    // [ sequence:1 ]
    // [ height:4 ]
    // [ block_hash:32 ]
    // [ tx:... ]
    const auto payload = build_chunk(
    {
        message::to_bytes(error::success),
        to_array(sequence),
        to_little_endian(height),
        block_hash,
        tx->to_data()
    });

    send(reply_to, address_update2, id, payload);
}

// Handlers.
// ----------------------------------------------------------------------------

bool notification_worker::handle_address(const code& ec,
    const binary& field, uint32_t height, const hash_digest& block_hash,
    transaction_const_ptr tx, const route& reply_to, uint32_t id,
    const binary& prefix_filter, sequence_ptr sequence)
{
    if (ec)
    {
        send(reply_to, address_update2, id, message::to_bytes(ec));
        return false;
    }

    if (prefix_filter.is_prefix_of(field))
    {
        send_address(reply_to, id, *sequence, height, block_hash, tx);
        ++(*sequence);
    }

    return true;
}

// Subscribers.
// ----------------------------------------------------------------------------

// Subscribe to address and stealth prefix notifications.
// Each delegate must connect to the appropriate query notification endpoint.
void notification_worker::subscribe_address(const route& reply_to, uint32_t id,
    const binary& prefix_filter, bool unsubscribe)
{
    static const auto error_code = error::channel_stopped;
    const address_key key(reply_to, prefix_filter);

    if (unsubscribe)
    {
        // Just as with an expiration (purge) this will cause the stored
        // handler (notification_worker::handle_address) to be invoked but
        // with the specified error code (error::channel_stopped) as
        // opposed to error::channel_timeout.
        address_subscriber_->unsubscribe(key, error_code, {}, 0, {}, {});
        return;
    }

    // The sequence enables the client to detect dropped messages.
    const auto sequence = std::make_shared<uint8_t>(0);
    const auto& duration = settings_.subscription_expiration();

    // This class must be kept in scope until work is terminated.
    auto handler =
        std::bind(&notification_worker::handle_address,
            this, _1, _2, _3, _4, _5, reply_to, id, prefix_filter,
                sequence);

    address_subscriber_->subscribe(std::move(handler), key, duration,
        error_code, {}, 0, {}, {});
}

////// Subscribe to transaction penetration notifications.
////// Each delegate must connect to the appropriate query notification endpoint.
////void notification_worker::subscribe_penetration(const route& reply_to,
////    uint32_t id, const hash_digest& tx_hash)
////{
////    // TODO:
////    // Height and hash are zeroized if tx is not chained (inv/mempool).
////    // If chained or penetration is 100 (percent) drop subscription.
////    // Only send messages at configured thresholds (e.g. 20/40/60/80/100%).
////    // Thresholding allows the server to mask its peer count.
////    // Penetration is computed by the relay handler.
////    // No sequence is required because gaps are okay.
////    // [ tx_hash:32 ]
////    // [ penetration:1 ]
////    // [ height:4 ]
////    // [ block_hash:32 ]
////    ////penetration_subscriber_->subscribe();
////}

// Notification (via blockchain).
// ----------------------------------------------------------------------------

bool notification_worker::handle_reorganization(const code& ec,
    size_t fork_height, block_const_ptr_list_const_ptr new_blocks,
    block_const_ptr_list_const_ptr)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling new block: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    // Blockchain height is 64 bit but obelisk protocol is 32 bit.
    auto fork_height32 = safe_unsigned<uint32_t>(fork_height);

    for (const auto block: *new_blocks)
        notify_block(safe_increment(fork_height32), block);

    return true;
}

void notification_worker::notify_block(uint32_t height,
    block_const_ptr block)
{
    if (stopped())
        return;

    const auto block_hash = block->header().hash();

    for (const auto& tx: block->transactions())
    {
        // TODO: use shared pointers for block members to avoid copying.
        auto pointer = std::make_shared<const bc::message::transaction>(tx);

        ////const auto tx_hash = tx->hash();
        notify_transaction(height, block_hash, pointer);
        ////notify_penetration(height, block_hash, tx_hash);
    }
}

// Notification (via transaction inventory).
// ----------------------------------------------------------------------------
// This relies on peers always notifying us of new txs via inv messages.

////// BUGBUG: this is disconnected from subscription.
////bool notification_worker::handle_inventories(const code& ec,
////    inventory_const_ptr packet)
////{
////    if (stopped() || ec == error::service_stopped)
////        return false;
////
////    if (ec)
////    {
////        LOG_WARNING(LOG_SERVER)
////            << "Failure handling inventory: " << ec.message();
////
////        // Don't let a failure here prevent future notifications.
////        return true;
////    }
////
////    // Loop inventories and extract transaction hashes.
////    for (const auto& inventory: packet->inventories())
////        if (inventory.is_transaction_type())
////            notify_inventory(inventory);
////
////    return true;
////}
////
////void notification_worker::notify_inventory(
////    const bc::message::inventory_vector& inventory)
////{
////    notify_penetration(0, null_hash, inventory.hash());
////}

// Notification (via mempool and blockchain).
// ----------------------------------------------------------------------------

bool notification_worker::handle_transaction_pool(const code& ec,
    transaction_const_ptr tx)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling new transaction: " << ec.message();

        // Don't let a failure here prevent future notifications.
        return true;
    }

    notify_transaction(0, null_hash, tx);
    return true;
}

// This parsing is duplicated by bc::database::data_base.
void notification_worker::notify_transaction(uint32_t height,
    const hash_digest& block_hash, transaction_const_ptr tx)
{
    uint32_t prefix;

    // TODO: move full integer and array constructors into binary.
    static constexpr size_t prefix_bits = sizeof(prefix) * byte_bits;
    static constexpr size_t address_bits = short_hash_size * byte_bits;
    const auto& outputs = tx->outputs();

    if (stopped() || outputs.empty())
        return;

    // see data_base::push_inputs
    // Loop inputs and extract payment addresses.
    for (const auto& input: tx->inputs())
    {
        // This is cached by database extraction (if indexed).
        const auto address = input.address();

        if (address)
        {
            const binary field(address_bits, address.hash());
            notify_address(field, height, block_hash, tx);
        }
    }

    // see data_base::push_outputs
    // Loop outputs and extract payment addresses.
    for (const auto& output: outputs)
    {
        // This is cached by database extraction (if indexed).
        const auto address = output.address();

        if (address)
        {
            const binary field(address_bits, address.hash());
            notify_address(field, height, block_hash, tx);
        }
    }

    // see data_base::push_stealth
    // Loop output pairs and extract stealth payments.
    for (size_t index = 0; index < (outputs.size() - 1); ++index)
    {
        const auto& ephemeral_script = outputs[index].script();
        const auto& payment_output = outputs[index + 1];

        // Try to extract a stealth prefix from the first output.
        // Try to extract the payment address from the second output.
        // The address is cached by database extraction (if indexed).
        if (payment_output.address() &&
            to_stealth_prefix(prefix, ephemeral_script))
        {
            const binary field(prefix_bits, to_little_endian(prefix));
            notify_address(field, height, block_hash, tx);
        }
    }
}

// v3
void notification_worker::notify_address(const binary& field, uint32_t height,
    const hash_digest& block_hash, transaction_const_ptr tx)
{
    static const auto code = error::success;
    address_subscriber_->relay(code, field, height, block_hash, tx);
}

////// v3.x
////void notification_worker::notify_penetration(uint32_t height,
////    const hash_digest& block_hash, const hash_digest& tx_hash)
////{
////    static const auto code = error::success;
////    penetration_subscriber_->relay(code, height, block_hash, tx_hash);
////}

} // namespace server
} // namespace libbitcoin
