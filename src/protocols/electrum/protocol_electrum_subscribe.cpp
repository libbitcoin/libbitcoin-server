/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
// Post to an independent strand on the network threadpool. This protects the
// subscriptions and ensures that the channel remains both cancellable and
// responsive. The channel listener remains paused during this call, which
// guards against call backlogging (DoS) and requires the monitor to allow
// socket cancellation and server stop to interrupt expensive query.

void protocol_electrum::handle_blockchain_scripthash_subscribe(const code& ec,
    rpc_interface::blockchain_scripthash_subscribe,
    const std::string& scripthash) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1) ||
         at_least(electrum::version::v1_7))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, scripthash))
    {
        send_code(error::electrum::bad_request);
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
        send_code(error::electrum::method_not_found);
        return;
    }

    monitor(true);
    POST_NOTIFY(do_scripthash_subscribe, hash, type);
}

void protocol_electrum::do_scripthash_subscribe(const hash_digest& hash,
    notify_t type) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    hash_digest status{};
    code ec{ error::electrum::excessive_resource_usage };
    if (address_subscriptions_.size() < options().maximum_subscriptions)
    {
        const auto at = address_subscriptions_.try_emplace(hash,
            address_subscription{ type });

        // Initial subscription is limited by configured maximum history.
        const auto limit = at.second ? options().maximum_history : max_size_t;
        if ((ec = get_scripthash_history(at.first->second, at.first->first, limit)))
        {
            if (at.second) address_subscriptions_.erase(at.first);
        }
        else
        {
            status = at.first->second.status;
            subscribed_address_.store(true, relaxed);
        }
    }

    POST(complete_scripthash_subscribe, ec, std::move(status));
}

void protocol_electrum::complete_scripthash_subscribe(const code& ec,
    const hash_digest& status) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        using namespace error::electrum;
        send_code(translate(ec, daemon_error));
        return;
    }

    send_result(status == null_hash ? value_t{} :
        value_t{ encode_base16(status) }, 128);
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

    if (!at_least(electrum::version::v1_4_2) ||
         at_least(electrum::version::v1_7))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, scripthash))
    {
        send_code(error::electrum::bad_request);
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
        send_code(error::electrum::method_not_found);
        return;
    }

    POST_NOTIFY(do_scripthash_unsubscribe, hash);
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
    send_result(found, 16);
}

// notify
// ----------------------------------------------------------------------------

bool protocol_electrum::handle_broadcast_transaction(const code& ec,
    const network::messages::peer::transaction::cptr& message,
    uint64_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // blockchain.transaction.get also depends on this (see broadcast_tx).
    retain_tx(message->transaction_ptr);

    // Notifications require a full duplex transport, as with handle_chase.
    if (!channel_->websocket() && !channel_->downgraded())
        return true;

    if (subscribed_address_.load(relaxed))
    {
        BC_ASSERT(archive().address_enabled());

        // retained() must be copied here, on the channel strand.
        POST_NOTIFY(do_broadcast_scripthash, message->transaction_ptr,
            retained());
    }

    return true;
}

// Notifier for blockchain_scripthash_subscribe events.
void protocol_electrum::do_scripthash(node::header_t) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (auto& [key, sub]: address_subscriptions_)
    {
        const auto previous = sub.status;
        if (const auto ec = get_scripthash_history(sub, key, max_size_t))
        {
            if (ec == database::error::query_canceled)
                return;

            if (ec != error::not_found)
            {
                LOGF("Electrum::do_scripthash, " << ec.message());
            }
        }
        else if (sub.status != previous)
        {
            POST(scripthash_notify, sub.status, key, sub.type);
        }
    }
}

// Recomputes and echoes status for the subscriptions the tx touches.
void protocol_electrum::do_broadcast_scripthash(
    const chain::transaction::cptr& tx,
    const retained_txs& retained_snapshot) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (auto& [key, sub]: address_subscriptions_)
    {
        if (!touches(*tx, key))
            continue;

        const auto previous = sub.status;
        if (const auto ec = get_scripthash_history(sub, key, max_size_t,
            retained_snapshot))
        {
            if (ec != database::error::query_canceled &&
                ec != error::not_found)
            {
                LOGF("Electrum::do_broadcast_scripthash, " << ec.message());
            }

            continue;
        }

        if (sub.status != previous)
            POST(scripthash_notify, sub.status, key, sub.type);
    }
}

void protocol_electrum::scripthash_notify(const hash_digest& status,
    const hash_digest& hash, notify_t type) NOEXCEPT
{
    BC_ASSERT(stranded());

    send_notification(to_method_name(type), array_t
    {
        encode_hash(hash),
        status == null_hash ? value_t{} : value_t{ encode_base16(status) }
    }, 128);
}

// utility
// ----------------------------------------------------------------------------

// private/static
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

// private/static
void protocol_electrum::write_status(midstate& accumulator,
    const history& history) NOEXCEPT
{
    // Height is zero (rooted) or max_size_t for unconfirmed history txs.
    accumulator.write(encode_hash(history.tx.hash()));
    accumulator.write(":");
    accumulator.write(std::to_string(to_signed(history.tx.height())));
    accumulator.write(":");
}

// protected
// extra is a channel-strand snapshot of retained(), passed by the caller,
// as this runs on notification_strand_ (see handle_broadcast_transaction).
code protocol_electrum::get_scripthash_history(address_subscription& sub,
    const hash_digest& hash, size_t limit,
    const retained_txs& extra) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    histories history{};
    const auto& query = archive();
    if (const auto ec = query.get_history(stopping_, sub.cursor, history, hash,
        limit, turbo_))
        return ec;

    // Same criteria as append_retained.
    for (const auto& [tx_hash, tx]: extra)
    {
        if (!query.to_tx(tx_hash).is_terminal() || !touches(*tx, hash))
            continue;

        history.push_back(database::history
        {
            { tx_hash, database::history::rooted_height },
            tx->fee(),
            database::history::unconfirmed_position
        });
    }

    if (history.empty())
        return error::success;

    if (!extra.empty())
        database::history::filter_sort_and_dedup(history);

    auto it = history.cbegin();
    while (it != history.cend() && it->confirmed())
        write_status(sub.accumulator, *it++);

    midstate copy = sub.accumulator;
    while (it != history.cend())
        write_status(copy, *it++);

    sub.status = copy.flush();
    return error::success;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
