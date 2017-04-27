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
#include <chrono>
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

// This causes addresses to match each subscription and stealth prefixes
// to match each subscription 24 times [minimum_filter..maximum_filter].
////#define HIGH_VOLUME_NOTIFICATION_TESTING

using namespace std::chrono;
using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::protocol;
using namespace bc::wallet;
using role = zmq::socket::role;

static const auto notification_address = "notification.address";
static const auto notification_stealth = "notification.stealth";

notification_worker::notification_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    security_(secure ? "secure" : "public"),
    settings_(node.server_settings()),
    external_(node.protocol_settings()),
    internal_(external_.send_high_water, external_.receive_high_water),
    worker_(query_service::worker_endpoint(secure)),
    authenticator_(authenticator),
    node_(node)
{
}

// There is no unsubscribe so this class shouldn't be restarted.
// Notifications are ordered by validation in node but thread safety is still
// required so that purge can run on a seperate time thread.
bool notification_worker::start()
{
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

// Implement worker as a dummy socket, for uniform stop implementation.
void notification_worker::work()
{
    zmq::socket dummy(authenticator_, role::pair);

    if (!started(dummy))
        return;

    const auto period = purge_milliseconds();
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

// Sending.
// The dealer blocks until the query service dealer is available.
// ----------------------------------------------------------------------------

zmq::socket::ptr notification_worker::connect()
{
    auto dealer = std::make_shared<zmq::socket>(authenticator_, role::dealer);

    // Connect to the query service worker endpoint.
    // This must be a dealer, since the response is asynchronous.
    auto ec = dealer->connect(worker_);

    if (ec && ec != error::service_stopped)
        LOG_WARNING(LOG_SERVER)
            << "Failed to connect " << security_ << " notification worker to "
            << worker_ << " : " << ec.message();

    // Using shared pointer because sockets cannot be copied.
    return dealer;
}

bool notification_worker::send(zmq::socket& dealer,
    const subscription& routing, const std::string& command,
    const code& status, size_t height, const hash_digest& tx_hash)
{
    // [ code:4 ]
    // [ sequence:2 ]
    // [ height:4 ]
    // [ tx hash:32 ]
    // Notifications are formatted as query response messages.
    ///////////////////////////////////////////////////////////////////////////
    message reply(routing, command, build_chunk(
    {
        message::to_bytes(status),
        to_little_endian(routing.sequence()),
        to_little_endian(static_cast<uint32_t>(height)),
        tx_hash
    }));
    ///////////////////////////////////////////////////////////////////////////

    const auto ec = reply.send(dealer);

    if (ec && ec != error::service_stopped)
        LOG_WARNING(LOG_SERVER)
            << "Failed to send notification to "
            << reply.route().display() << " " << ec.message();

    // Failure could create large number of warnings so return false to stop.
    return ec == error::success;
}

// Notification (via blockchain).
// ----------------------------------------------------------------------------

bool notification_worker::handle_reorganization(const code& ec,
    size_t fork_height, block_const_ptr_list_const_ptr incoming,
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

    // Nothing to do here, a channel is stopping.
    if (!incoming || incoming->empty())
        return true;

    // Do not announce addresses to clients if too far behind.
    if (node_.chain().is_stale())
        return true;

    if (address_subscriptions_empty() && stealth_subscriptions_empty())
        return true;

    // Connect failure is logged in connect, nothing else to do on fail.
    auto dealer = connect();

    if (!dealer)
        return true;

    for (const auto block: *incoming)
        notify_block(*dealer, safe_add(fork_height, size_t(1)), block);

    return true;
}

void notification_worker::notify_block(zmq::socket& dealer, size_t height,
    block_const_ptr block)
{
    if (stopped())
        return;

    for (const auto& tx: block->transactions())
        notify_transaction(dealer, height, tx);
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

    // Nothing to do here.
    if (!tx)
        return true;

    // Do not announce addresses to clients if too far behind.
    if (node_.chain().is_stale())
        return true;

    if (address_subscriptions_empty() && stealth_subscriptions_empty())
        return true;

    // Connect failure is logged in connect, nothing else to do on fail.
    auto dealer = connect();

    if (!dealer)
        return true;

    // Use zero height as sentinel for unconfirmed transaction.
    notify_transaction(*dealer, 0, *tx);
    return true;
}

// All payment addresses are cached on the transaction.
// This parsing is duplicated by bc::database::data_base.
void notification_worker::notify_transaction(zmq::socket& dealer,
    size_t height, const transaction& tx)
{
    if (stopped())
        return;

    const auto& outputs = tx.outputs();

    if (outputs.empty())
        return;

    // Gather unique values, eliminating duplicate notifications per tx.
    stealth_set prefixes;
    address_set addresses;

    if (!address_subscriptions_empty())
    {
        for (const auto& input: tx.inputs())
        {
            // TODO: use a vector result to extract sign_multisig.
            const auto address = input.address();

            if (address)
                addresses.insert(address.hash());
        }

        for (const auto& output: outputs)
        {
            // TODO: use a vector result to extract pay_multisig.
            // TODO: notify all multisig participants using prevout addresses.
            const auto address = output.address();

            if (address)
                addresses.insert(address.hash());
        }
    }

    if (!stealth_subscriptions_empty())
    {
        for (size_t index = 0; index < (outputs.size() - 1); ++index)
        {
            uint32_t prefix;
            const auto& even_script = outputs[index + 0].script();
            const auto& odd_output = outputs[index + 1];

            if (odd_output.address() && to_stealth_prefix(prefix, even_script))
                prefixes.insert(prefix);
        }
    }

    // Send both sets of notifications on the same worker connection.
    notify(dealer, addresses, prefixes, height, tx.hash());
}

void notification_worker::notify(zmq::socket& dealer,
    const address_set& hashes, const stealth_set& prefixes, size_t height,
    const hash_digest& tx_hash)
{
    static const code ok = error::success;

    if (stopped())
        return;

    // Accumulate updates, send notifications outside locks.
    std::vector<subscription> notifies;

    // Notify address subscribers, O(N + M).
    if (!hashes.empty())
    {
        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        shared_lock lock(address_mutex_);

        const auto& left = address_subscriptions_.left;

        for (const auto& hash: hashes)
        {
#ifdef HIGH_VOLUME_NOTIFICATION_TESTING
            for (auto it = left.begin(); it != left.end(); ++it)
#else
            auto range = left.equal_range(hash);
            for (auto it = range.first; it != range.second; ++it)
#endif
            {
                it->second.increment();
                notifies.push_back(it->second);
            }
        }
        ///////////////////////////////////////////////////////////////////////
    }

    // Send failure is logged in send.
    for (auto& notify: notifies)
        if (!send(dealer, notify, notification_address, ok, height, tx_hash))
            break;

    notifies.clear();

    // Notify stealth subscribers, O(24 * (N + M)).
    if (!prefixes.empty())
    {
        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        shared_lock lock(stealth_mutex_);

        const auto& left = stealth_subscriptions_.left;

        for (const auto& prefix: prefixes)
        {
            for (auto bits = stealth_address::min_filter_bits;
                bits <= stealth_address::max_filter_bits; ++bits)
            {
#ifdef HIGH_VOLUME_NOTIFICATION_TESTING
                for (auto it = left.begin(); it != left.end(); ++it)
#else
                auto range = left.equal_range(binary{ bits, prefix });
                for (auto it = range.first; it != range.second; ++it)
#endif
                {
                    it->second.increment();
                    notifies.push_back(it->second);
                }
            }
        }
        ///////////////////////////////////////////////////////////////////////
    }

    // Send failure is logged in send.
    for (auto& notify: notifies)
        if (!send(dealer, notify, notification_stealth, ok, height, tx_hash))
            break;
}

// Subscription.
// ----------------------------------------------------------------------------

time_t notification_worker::current_time()
{
    // use system_clock to ensure to_time_t is defined.
    const auto now = system_clock::now();
    return system_clock::to_time_t(system_clock::now());
}

time_t notification_worker::cutoff_time() const
{
    // In case a purge call is made, nothing will be puged (until rollover).
    if (settings_.subscription_expiration_minutes == 0)
        return max_int32;

    // use system_clock to ensure to_time_t is defined.
    const auto now = system_clock::now();
    const auto period = minutes(settings_.subscription_expiration_minutes);
    return system_clock::to_time_t(now - period);
}

int32_t notification_worker::purge_milliseconds() const
{
    // This results in infinite polling and therefore no purge calls.
    if (settings_.subscription_expiration_minutes == 0)
        return -1;

    const int64_t minutes = settings_.subscription_expiration_minutes;
    const int64_t milliseconds = minutes * 60 * 1000;
    auto capped = std::min(milliseconds, static_cast<int64_t>(max_int32));
    return static_cast<int32_t>(capped);
}

void notification_worker::purge()
{
    static const code to = error::channel_timeout;

    // Connect failure is logged in connect (purge regardless).
    auto dealer = connect();

    // Purge any subscription with an update time earlier than this.
    const auto cutoff = cutoff_time();

    // Accumulate removals, send expiration notifications outside locks.
    std::vector<subscription> expires;

    if (true)
    {
        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        unique_lock lock(address_mutex_);

        auto& right = address_subscriptions_.right;

        for (auto it = right.begin(); it != right.end() &&
            it->first.updated() < cutoff; it = right.erase(it))
        {
            it->first.increment();
            expires.push_back(it->first);
        }
        ///////////////////////////////////////////////////////////////////////
    }

    // Send failure is logged in send.
    if (dealer)
        for (auto& expire: expires)
            if (!send(*dealer, expire, notification_address, to, 0, null_hash))
                break;

    expires.clear();

    if (true)
    {
        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        unique_lock lock(stealth_mutex_);

        auto& right = stealth_subscriptions_.right;

        for (auto it = right.begin(); it != right.end() &&
            it->first.updated() < cutoff; it = right.erase(it))
        {
            it->first.increment();
            expires.push_back(it->first);
        }
        ///////////////////////////////////////////////////////////////////////
    }

    // Send failure is logged in send.
    if (dealer)
        for (auto& expire: expires)
            if (!send(*dealer, expire, notification_stealth, to, 0, null_hash))
                break;
}

bool notification_worker::address_subscriptions_empty() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(address_mutex_);

    return address_subscriptions_.empty();
    ///////////////////////////////////////////////////////////////////////////
}

bool notification_worker::stealth_subscriptions_empty() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(stealth_mutex_);

    return stealth_subscriptions_.empty();
    ///////////////////////////////////////////////////////////////////////////
}

code notification_worker::subscribe_address(const message& request,
    short_hash&& address_hash, bool unsubscribe)
{
    if (stopped())
        return error::service_stopped;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    address_mutex_.lock_upgrade();

    auto& left = address_subscriptions_.left;
    auto range = left.equal_range(address_hash);

    // Check each subscription for the given address hash.
    // A change to the id is not considered (caller should not change).
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == request.route())
        {
            address_mutex_.unlock_upgrade_and_lock();
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

            if (unsubscribe)
                left.erase(it);
            else
                it->second.set_updated(current_time());

            //-----------------------------------------------------------------
            address_mutex_.unlock();
            return error::success;
        }
    }

    // TODO: add independent limits for stealth and address.
    if (address_subscriptions_.size() >= settings_.subscription_limit)
    {
        address_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return error::oversubscribed;
    }

    address_mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    address_subscriptions_.insert({
        std::move(address_hash),
        subscription{ request.route(), request.id(), current_time() }
    });

    address_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
    return error::success;
}

code notification_worker::subscribe_stealth(const message& request,
    binary&& prefix_filter, bool unsubscribe)
{
    if (stopped())
        return error::service_stopped;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    stealth_mutex_.lock_upgrade();

    auto& left = stealth_subscriptions_.left;
    auto range = left.equal_range(prefix_filter);

    // Check each subscription for the given prefix filter.
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == request.route())
        {
            stealth_mutex_.unlock_upgrade_and_lock();
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

            if (unsubscribe)
                left.erase(it);
            else
                it->second.set_updated(current_time());

            //-----------------------------------------------------------------
            stealth_mutex_.unlock();
            return error::success;
        }
    }

    // TODO: add independent limits for stealth and address.
    if (stealth_subscriptions_.size() >= settings_.subscription_limit)
    {
        stealth_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return error::oversubscribed;
    }

    stealth_mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    stealth_subscriptions_.insert({
        std::move(prefix_filter),
        subscription{ request.route(), request.id(), current_time() }
    });

    stealth_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
    return error::success;
}

} // namespace server
} // namespace libbitcoin
