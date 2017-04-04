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

namespace libbitcoin {
namespace server {

#define NAME "notification_worker"

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::protocol;
using namespace bc::wallet;

// Notifications respond with commands that are distinct from the subscription.
static const std::string address_update2("address.update2");

notification_worker::notification_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    settings_(node.server_settings()),
    node_(node),
    authenticator_(authenticator),
    address_subscriber_(std::make_shared<address_subscriber>(
        node.thread_pool(), NAME "_address"))
{
}

// There is no unsubscribe so this class shouldn't be restarted.
bool notification_worker::start()
{
    address_subscriber_->start();

    // Subscribe to blockchain reorganizations.
    node_.subscribe_blockchain(
        std::bind(&notification_worker::handle_reorganization,
            this, _1, _2, _3, _4));

    // Subscribe to transaction pool acceptances.
    node_.subscribe_transaction(
        std::bind(&notification_worker::handle_transaction_pool,
            this, _1, _2));

    return zmq::worker::start();
}

// No unsubscribe so must be kept in scope until subscriber stop complete.
// Because of closures in subscriber, must call stop from node stop handler.
bool notification_worker::stop()
{
    address_subscriber_->stop();

    // Unlike purge, stop will not propagate, since the context is closed.
    address_subscriber_->invoke(error::service_stopped, {}, 0, {}, {});

    return zmq::worker::stop();
}

// Implement worker as a dummy socket, for uniform stop implementation.
void notification_worker::work()
{
    zmq::socket dummy(authenticator_, zmq::socket::role::pair);

    if (!started(dummy))
        return;

    const auto period = purge_interval_milliseconds();
    zmq::poller poller;
    poller.add(dummy);

    // We do not send/receive on poller, we use it for purge and context stop.
    // Other threads connect dynamically to query service to send notifcation.
    // BUGBUG: stop is insufficient to stop worker, because of long period.
    while (!poller.terminated() && !stopped())
    {
        poller.wait(period);
        purge();
    }

    finished(dummy.stop());
}

// Pruning.
// ----------------------------------------------------------------------------

int32_t notification_worker::purge_interval_milliseconds() const
{
    const int64_t minutes = settings_.subscription_expiration_minutes;
    const int64_t milliseconds = minutes * 60 * 1000;
    auto capped = std::min(milliseconds, static_cast<int64_t>(max_int32));
    return static_cast<int32_t>(capped);
}

// Signal expired subscriptions to self-remove.
void notification_worker::purge()
{
    address_subscriber_->purge(error::channel_timeout, {}, 0, {}, {});
}

// Sending.
// The dealer blocks until the query service dealer is available.
// ----------------------------------------------------------------------------

// This cannot invoke any method of the subscriber or it will deadlock.
void notification_worker::send(const route& reply_to,
    const std::string& command, uint32_t id, const data_chunk& payload)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? query_service::secure_worker :
        query_service::public_worker;

    // This must be a dealer, since the response is asynchronous.
    zmq::socket notifier(authenticator_, zmq::socket::role::dealer);

    // This must not block if endpoint has been stopped (or would deadlock).
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

// Handlers.
// ----------------------------------------------------------------------------

// This cannot invoke any method of the subscriber or it will deadlock.
bool notification_worker::handle_address(const code& ec,
    const binary& field, uint32_t height, const hash_digest& block_hash,
    transaction_const_ptr tx, const route& reply_to, uint32_t id,
    const binary& prefix_filter, sequence_ptr sequence)
{
    if (ec)
    {
        // [ code:4 ]
        send(reply_to, address_update2, id, message::to_bytes(ec));
        return false;
    }

    if (prefix_filter.is_prefix_of(field))
    {
        // [ code:4 ]
        // [ sequence:2 ]
        // [ height:4 ]
        // [ block_hash:32 ]
        // [ tx:... ]
        send(reply_to, address_update2, id, build_chunk(
        {
            message::to_bytes(error::success),
            to_little_endian(*sequence),
            to_little_endian(height),
            block_hash,
            tx->to_data(bc::message::version::level::canonical)
        }));

        ++(*sequence);
    }

    return true;
}

// Subscribers.
// ----------------------------------------------------------------------------

// Subscribe to address and stealth prefix notifications.
// Each delegate must connect to the appropriate query notification endpoint.
code notification_worker::subscribe_address(const route& reply_to, uint32_t id,
    const binary& prefix_filter, bool unsubscribe)
{
    const address_key key(reply_to, prefix_filter);

    if (unsubscribe)
    {
        // Cause stored handler to be invoked but with specified error code.
        address_subscriber_->unsubscribe(key, error::service_stopped, {}, 0,
            {}, {});
        return error::success;
    }

    // This allows resubscriptions at the service limit.
    if (address_subscriber_->limited(key, settings_.subscription_limit))
        return error::oversubscribed;

    // The sequence enables the client to detect dropped messages.
    const auto sequence = std::make_shared<uint16_t>(0);
    const auto& duration = settings_.subscription_expiration();

    auto handler =
        std::bind(&notification_worker::handle_address,
            this, _1, _2, _3, _4, _5, reply_to, id, prefix_filter,
                sequence);

    // If the service is stopped a notification will result.
    address_subscriber_->subscribe(std::move(handler), key, duration,
        error::service_stopped, {}, 0, {}, {});
    return error::success;
}

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

    if (address_subscriber_->empty())
        return true;

    // Blockchain height is size_t but obelisk protocol is 32 bit.
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
        notify_transaction(height, block_hash, pointer);
    }
}

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

    if (address_subscriber_->empty())
        return true;

    notify_transaction(0, null_hash, tx);
    return true;
}

// This parsing is duplicated by bc::database::data_base.
void notification_worker::notify_transaction(uint32_t height,
    const hash_digest& block_hash, transaction_const_ptr tx)
{
    // TODO: move full integer and array constructors into binary.
    static constexpr size_t prefix_bits = sizeof(uint32_t) * byte_bits;
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
        uint32_t prefix;
        const auto& even_output = outputs[index + 0];
        const auto& odd_output = outputs[index + 1];

        // Try to extract a stealth prefix from the first output.
        // Try to extract the payment address from the second output.
        // The address is cached by database extraction (if indexed).
        if (odd_output.address() &&
            to_stealth_prefix(prefix, even_output.script()))
        {
            const binary field(prefix_bits, to_little_endian(prefix));
            notify_address(field, height, block_hash, tx);
        }
    }
}

void notification_worker::notify_address(const binary& field, uint32_t height,
    const hash_digest& block_hash, transaction_const_ptr tx)
{
    address_subscriber_->relay(error::success, field, height, block_hash, tx);
}

} // namespace server
} // namespace libbitcoin
