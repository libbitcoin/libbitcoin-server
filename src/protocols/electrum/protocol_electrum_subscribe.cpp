/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// subscribe
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_subscribe(const code& ec,
    rpc_interface::blockchain_scripthash_subscribe,
    const std::string& scripthash) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, scripthash))
    {
        send_code(error::invalid_argument);
        return;
    }

    scripthash_subscribe(hash, notify_t::scripthash);
}

// common
void protocol_electrum::scripthash_subscribe(const hash_digest& hash,
    notify_t type) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    // Post to an independent strand on the network threadpool. This protects
    // the subscriptions and ensures that the channel remains both cancellable
    // and responsive. The channel listener remains paused during this call,
    // which guards against call backlogging (DoS) and requires the monitor to
    // allow socket cancellation and server stop to interrupt expensive query.
    monitor(true);
    NOTIFY(do_scripthash_subscribe, hash, type);
}

// Subscription response is idempotent.
void protocol_electrum::do_scripthash_subscribe(const hash_digest& hash,
    notify_t type) NOEXCEPT
{
    // Cancellability is preserved because not on channel strand.
    BC_ASSERT(notification_strand_.running_in_this_thread());

    hash_digest status{};
    code ec{ error::subscription_limit };
    if (address_subscriptions_.size() < options_.maximum_subscriptions)
    {
        const auto at = address_subscriptions_
            .try_emplace(hash, address_subscription{ type });

        // Initial subscription is limited by configured maximum history.
        const auto limit = at.second ? options().maximum_history : max_size_t;

        // Partially-cached result idempotent (new or redundant subscription).
        ec = get_scripthash_history(at.first->second, at.first->first, limit);
        status = at.first->second.status;
        subscribed_address_.store(true, relaxed);
    }

    POST(complete_scripthash_subscribe, ec, hash, std::move(status));
}

void protocol_electrum::complete_scripthash_subscribe(const code& ec,
    const hash_digest& status, const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        // Limited is only expected code, not found is success/null_hash.
        send_code(ec);
        return;
    }

    send_result(array_t
    {
        encode_hash(hash),
        status == null_hash ? value_t{} : value_t{ encode_hash(status) }
    }, 128, BIND(complete, _1));
}

// unsubscribe
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_unsubscribe(const code& ec,
    rpc_interface::blockchain_scripthash_unsubscribe,
    const std::string& scripthash) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4_2))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, scripthash))
    {
        send_code(error::invalid_argument);
        return;
    }

    scripthash_unsubscribe(hash);
}

// common
void protocol_electrum::scripthash_unsubscribe(
    const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    NOTIFY(do_scripthash_unsubscribe, hash);
}

void protocol_electrum::do_scripthash_unsubscribe(
    const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    const auto found = to_bool(address_subscriptions_.erase(hash));
    if (is_zero(address_subscriptions_.size()))
        subscribed_address_.store(false, relaxed);

    POST(complete_scripthash_unsubscribe, found);
}

void protocol_electrum::complete_scripthash_unsubscribe(bool found) NOEXCEPT
{
    send_result(found, 16, BIND(complete, _1));
}

// notify
// ----------------------------------------------------------------------------

// Notifier for blockchain_scripthash_subscribe events.
void protocol_electrum::do_scripthash(node::header_t) NOEXCEPT
{
    // Cancellability is preserved because not on channel strand.
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (auto& [key, sub]: address_subscriptions_)
    {
        // Depth limit is never imposed once a subscription is accepted.
        if (const auto ec = get_scripthash_history(sub, key))
        {
            if (ec == database::error::query_canceled)
                return;

            if (ec == error::not_found)
                continue;

            LOGF("Electrum::do_scripthash, " << ec.message());
        }
        else
        {
            // Asio-buffered message (small, not under caller control).
            POST(scripthash_notify, sub.status, key, sub.type);
        }
    }
}

void protocol_electrum::scripthash_notify(const hash_digest& status,
    const hash_digest& hash, notify_t type) NOEXCEPT
{
    BC_ASSERT(stranded());

    send_notification(to_method_name(type), array_t
    {
        encode_hash(hash),
        status == null_hash ? value_t{} : value_t{ encode_hash(status) }
    }, 128, BIND(handle_send, _1));
}

// utility
// ----------------------------------------------------------------------------
// private

// static
// Convert enumeration to json-rpc notification method name.
std::string protocol_electrum::to_method_name(notify_t type) NOEXCEPT
{
    switch (type)
    {
        case notify_t::address:
            return "blockchain.address.subscribe";
        case notify_t::scripthash:
            return "blockchain.scripthash.subscribe";
        default:
        case notify_t::scriptpubkey:
            return "blockchain.scriptpubkey.subscribe";
    }
}

// static
// Height is zero (rooted) or max_size_t for unconfirmed history txs.
void protocol_electrum::write_status(midstate& accumulator,
    const history& history) NOEXCEPT
{
    accumulator.write(encode_hash(history.tx.hash()));
    accumulator.write(":");
    accumulator.write(std::to_string(to_signed(history.tx.height())));
    accumulator.write(":");
}

code protocol_electrum::get_scripthash_history(address_subscription& sub,
    const hash_digest& hash, size_t limit) NOEXCEPT
{
    histories records{};
    const auto& query = archive();

    if (sub.cursor.is_terminal())
    {
        // Initial scan queries all confirmed and unconfired together.
        // Initial scan is depth-limited (based on config), others are not.
        if (const auto ec = query.get_history(stopping_, sub.cursor, records,
            hash, limit, turbo_))
            return ec;

        // Accumulate confirmed status in order.
        auto it = records.cbegin();
        const auto cend = records.cend();
        while (it != cend && it->confirmed())
            write_status(sub.accumulator, *it++);

        BC_ASSERT(std::none_of(it, cend, [](const auto& at)
            { return at.confirmed(); }));

        // Copy midstate accumulator and write unconfirmeds.
        midstate copy = sub.accumulator;
        while (it != cend)
            write_status(copy, *it++);

        // Flush, cache and return status (always updated on initial).
        sub.status = copy.flush();
        return error::success;
    }
    else
    {
        // Update scan queries new (cursor) confirmed independently.
        if (const auto ec = query.get_confirmed_history(stopping_, sub.cursor,
            records, hash, max_size_t, turbo_))
            return ec;

        // Accumulate confirmed status in order.
        auto it = records.cbegin();
        auto cend = records.cend();
        while (it != cend && it->confirmed())
            write_status(sub.accumulator, *it++);

        // Copy midstate accumulator for write of unconfirmeds.
        midstate copy = sub.accumulator;
        records.clear();

        // Update scan queries all unconfirmed independently.
        if (const auto ec = query.get_unconfirmed_history(stopping_, records,
            hash, turbo_))
            return ec;

        // Reinitialize iterator for unconfirmed writer.
        it = records.cbegin();
        cend = records.cend();

        // Accumulate unconfirmed status in order.
        while (it != cend)
            write_status(copy, *it++);

        // Flush, cache and return not found if no writes.
        auto status = copy.flush();
        if (sub.status == status)
            return error::not_found;

        // Set cache into midstate object for next run.
        sub.status = std::move(status);
        return error::success;
    }
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
