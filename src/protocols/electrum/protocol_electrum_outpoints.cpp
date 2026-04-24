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

#include <memory>
#include <ranges>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_UNIQUE_PTR)

void protocol_electrum::handle_blockchain_utxo_get_address(const code& ec,
    rpc_interface::blockchain_utxo_get_address, const std::string& tx_hash,
    double index) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t offset{};
    hash_digest hash{};
    if (!to_integer(offset, index) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto tx = query.to_tx(hash);
    if (tx.is_terminal())
    {
        ////send_code(error::not_found);
        send_result(null_t{}, 42, BIND(complete, _1));
        return;
    }

    const auto output = query.to_output(tx, offset);
    const auto script = query.get_output_script(output);
    if (!script)
    {
        send_code(error::server_error);
        return;
    }

    const auto address = extract_address(*script);
    if (!address)
    {
        send_result(null_t{}, 42, BIND(complete, _1));
        return;
    }

    send_result(address.encoded(), 56, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_outpoint_get_status(const code& ec,
    rpc_interface::blockchain_outpoint_get_status, const std::string& tx_hash,
    double txout_idx, const std::string& spk_hint) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t index{};
    hash_digest hash{};
    if (!to_integer(index, txout_idx) || !decode_hash(hash, tx_hash) ||
        !is_valid_hint(spk_hint))
    {
        send_code(error::invalid_argument);
        return;
    }

    outpoint_subscription sub{};
    get_outpoint_history(sub, { hash, index });

    // Sends first spender only, empty if not found.
    send_result(to_outpoint_status(sub), 128, BIND(complete, _1));
}

// subscribe
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_outpoint_subscribe(const code& ec,
    rpc_interface::blockchain_outpoint_subscribe, const std::string& tx_hash,
    double txout_idx, const std::string& spk_hint) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t index{};
    hash_digest hash{};
    if (!to_integer(index, txout_idx) || !decode_hash(hash, tx_hash) ||
        !is_valid_hint(spk_hint))
    {
        send_code(error::invalid_argument);
        return;
    }

    monitor(true);
    NOTIFY(do_outpoint_subscribe, point{ hash, index });
}

// Subscription response is idempotent.
void protocol_electrum::do_outpoint_subscribe(const point& prevout) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    outpoint_subscription sub{};
    code ec{ error::subscription_limit };
    if (outpoint_subscriptions_.size() < options().maximum_subscriptions)
    {
        ec = error::success;
        get_outpoint_history(sub, prevout);
        outpoint_subscriptions_.emplace(prevout, sub);
        subscribed_outpoint_.store(true, relaxed);
    }

    // All current subscribers are cached and forwarded.
    POST(complete_outpoint_subscribe, ec, std::move(sub), prevout);
}

void protocol_electrum::complete_outpoint_subscribe(const code& ec,
    const outpoint_subscription& sub, const point& prevout) NOEXCEPT
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

    // Send first spender only.
    send_result(to_outpoint_status(sub), 128, BIND(complete, _1));

    // Send remaining spenders as notifications.
    // Send here vs. completion handler because asio will queue it up
    // behind the send anyway, and this prevents another sub copy.
    if (!sub.spenders.empty())
    {
        const auto height = sub.outpoint.tx.height();
        for (auto spender = std::next(sub.spenders.begin());
            spender != sub.spenders.end(); ++spender)
        {
            POST(outpoint_notify, make_status(height, *spender), prevout);
        }
    }
}

// unsubscribe
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_outpoint_unsubscribe(const code& ec,
    rpc_interface::blockchain_outpoint_unsubscribe, const std::string& tx_hash,
    double txout_idx) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t index{};
    hash_digest hash{};
    if (!to_integer(index, txout_idx) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    NOTIFY(do_outpoint_unsubscribe, point{ hash, index });
}

void protocol_electrum::do_outpoint_unsubscribe(const point& prevout) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    const auto found = to_bool(outpoint_subscriptions_.erase(prevout));
    if (is_zero(outpoint_subscriptions_.size()))
        subscribed_outpoint_.store(false, relaxed);

    POST(complete_outpoint_unsubscribe, found);
}

void protocol_electrum::complete_outpoint_unsubscribe(bool found) NOEXCEPT
{
    send_result(found, 16, BIND(complete, _1));
}

// notify
// ----------------------------------------------------------------------------

// Notifier for blockchain_outpoint_subscribe events.
void protocol_electrum::do_outpoint(node::header_t) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (auto& subscription: outpoint_subscriptions_)
    {
        if (stopping_)
            return;

        auto& sub = subscription.second;
        const auto& prevout = subscription.first;

        outpoint_subscription result{};
        if (!get_outpoint_history(result, prevout))
        {
            LOGV("Electrum::do_outpoint, outpoint not found.");
            continue;
        }

        // There is no change.
        if (sub == result) continue;

        const auto height = sub.outpoint.tx.height();
        if (!sub.outpoint.valid() || sub.outpoint.tx.height() != height)
        {
            // Outpoint changed (or newly found), send all current spenders.
            if (result.spenders.empty())
            {
                POST(outpoint_notify, make_status(height), prevout);
            }
            else for (const auto& spender: result.spenders)
            {
                POST(outpoint_notify, make_status(height, spender), prevout);
            }
        }
        else
        {
            // Outpoint unchanged, send only new or changed spenders.
            for (const auto& spender: difference(result.spenders, sub.spenders))
            {
                POST(outpoint_notify, make_status(height, spender), prevout);
            }
        }

        // Update subscription state.
        sub = std::move(result);
    }
}

void protocol_electrum::outpoint_notify(const std::unique_ptr<object_t>& status,
    const point& prevout) NOEXCEPT
{
    BC_ASSERT(stranded());

    send_notification("blockchain.outpoint.subscribe", array_t
    {
        array_t{ encode_hash(prevout.hash()), prevout.index() },
        std::move(*status)
    }, 128, BIND(handle_send, _1));
}

// utility
// ----------------------------------------------------------------------------

// private/static
object_t protocol_electrum::to_outpoint_status(size_t output_height) NOEXCEPT
{
    return
    {
        { "height", to_unsigned(output_height) }
    };
}

// private/static
object_t protocol_electrum::to_outpoint_status(size_t output_height,
    const history& history) NOEXCEPT
{
    return
    {
        { "height", to_unsigned(output_height) },
        { "spender_txhash", encode_hash(history.tx.hash()) },
        { "spender_height", to_unsigned(history.tx.height()) }
    };
}

// private/static
object_t protocol_electrum::to_outpoint_status(
    const outpoint_subscription& sub) NOEXCEPT
{
    // Return empty object for not found.
    if (!sub.outpoint.valid())
        return {};

    // Converts only the first (if any).
    const auto output_height = sub.outpoint.tx.height();
    return sub.spenders.empty() ?
        to_outpoint_status(output_height) :
        to_outpoint_status(output_height, sub.spenders.front());
}

// protected
bool protocol_electrum::get_outpoint_history(outpoint_subscription& out,
    const point& prevout) const NOEXCEPT
{
    const auto& query = archive();
    out.outpoint = query.get_tx_history(query.to_tx(prevout.hash()));
    if (!out.outpoint.valid())
    {
        out.spenders.clear();
        return false;
    }

    out.spenders = query.get_spenders_history(prevout);
    return true;
}

// private/static
bool protocol_electrum::is_valid_hint(const std::string& hint) NOEXCEPT
{
    if (!hint.empty())
    {
        data_chunk bytes{};
        if (!decode_base16(bytes, hint))
            return false;

        chain::script script{ std::move(bytes), false };
        if (!script.is_valid())
            return false;
    }

    return true;
}

// private
std::unique_ptr<protocol_electrum::object_t> protocol_electrum::make_status(
    size_t output_height) const NOEXCEPT
{
    return std::make_unique<object_t>(to_outpoint_status(output_height));
}

// private
std::unique_ptr<protocol_electrum::object_t> protocol_electrum::make_status(
    size_t output_height, const history& history) const NOEXCEPT
{
    return std::make_unique<object_t>(to_outpoint_status(output_height,
        history));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
