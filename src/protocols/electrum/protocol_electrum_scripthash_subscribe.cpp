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

void protocol_electrum::do_scripthash_subscribe(const hash_digest& hash,
    notify_t type) NOEXCEPT
{
    // Cancellability is preserved because not on channel strand.
    BC_ASSERT(notification_strand_.running_in_this_thread());

    code ec{};
    hash_digest status{};
    if (address_subscriptions_.size() < options_.maximum_subscriptions)
    {
        // Subscription response is idempotent.
        auto at = address_subscriptions_.try_emplace(hash, type, midstate{});
        ec = get_scripthash_status(status, at.first->second, at.first->first);
        subscribed_address_.store(true, relaxed);
    }
    else
    {
        ec = error::subscription_limit;
    }

    POST(complete_scripthash_subscribe, ec, hash, status);
}

void protocol_electrum::complete_scripthash_subscribe(const code& ec,
    hash_digest& status, const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(stranded());
    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
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
        hash_digest status{};
        if (const auto ec = get_scripthash_status(status, sub, key))
        {
            if (ec == database::error::canceled) return;
            LOGF("Electrum::do_scripthash, " << ec.message());
        }
        else
        {
            // Asio-buffered message (small, not under caller control).
            POST(scripthash_notify, status, key, sub.type);
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

// regress
// ----------------------------------------------------------------------------

// The chain has regressed, clear all midstate cache and cursors.
void protocol_electrum::do_regressed(node::header_t) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (auto& [key, sub]: address_subscriptions_)
    {
        // writer.flush resets hash accumulator, sub.type remains unchanged.
        sub.state.writer.flush();
        sub.cursor = {};
    }
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
// TODO: this can be implemented as electrum json serializer (see bitcoind).
hash_digest protocol_electrum::to_status(const histories& histories) NOEXCEPT
{
    if (histories.empty())
        return {};

    midstate out{};
    for (const auto& record: histories)
    {
        out.writer.write_string(encode_hash(record.tx.hash()));
        out.writer.write_string(":");
        out.writer.write_string(std::to_string(to_signed(record.tx.height())));
        out.writer.write_string(":");
    }

    out.writer.flush();
    return out.status;
}

code protocol_electrum::get_scripthash_status(hash_digest& out,
    subscription& /* sub */, const hash_digest& hash) NOEXCEPT
{
    // TODO: use cursors and midstate to optimize succesive queries.
    ////auto& state = sub.state;
    ////const auto& point = sub.point_cursor;
    ////const auto& address = sub.address_cursor;

    histories histories{};
    const auto& query = archive();
    const auto ec = query.get_history(stopping_, histories, hash, turbo_);
    if (!ec) out = to_status(histories);
    return ec;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
