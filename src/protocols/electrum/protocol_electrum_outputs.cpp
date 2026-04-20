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

#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace system::chain;
using namespace network::rpc;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

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
    if (!to_integer(index, txout_idx) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    send_outpoint_status({ hash, index }, spk_hint);
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
    if (!to_integer(index, txout_idx) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    // Outpoint status is not long-running so the notification strand is only
    // used to guard the notifications set. No need for the monitor, as
    // do_outpoint_subscribe() is trivial and pointless to cancel.
    ////monitor(true);
    NOTIFY(do_outpoint_subscribe, point{ hash, index }, spk_hint);
}

void protocol_electrum::do_outpoint_subscribe(const point& prevout,
    const std::string& hint) NOEXCEPT
{
    // Cancellability is preserved because not on channel strand.
    BC_ASSERT(notification_strand_.running_in_this_thread());

    code ec{};
    if (outpoint_subscriptions_.size() < options_.maximum_subscriptions)
    {
        // Subscription response is idempotent.
        outpoint_subscriptions_.insert(prevout);
        subscribed_outpoint_.store(true, relaxed);
    }
    else
    {
        ec = error::subscription_limit;
    }

    POST(complete_outpoint_subscribe, ec, prevout, hint);
}

void protocol_electrum::complete_outpoint_subscribe(const code& ec,
    const point& prevout, const std::string& hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    ////monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        send_code(ec);
        return;
    }

    send_outpoint_status(prevout, hint);
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
    // Cancellability is preserved because not on channel strand.
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (const auto& prevout: outpoint_subscriptions_)
    {
        if (stopping_)
            return;

        auto status = std::make_unique<object_t>();
        if (!get_outpoint_status(*status, prevout))
        {
            LOGV("Electrum::do_outpoint, outpoint not found.");
        }
        else
        {
            // Asio-buffered message (small, not under caller control).
            POST(outpoint_notify, std::move(status), prevout);
        }
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

bool protocol_electrum::get_outpoint_status(object_t& status,
    const point& prevout) const NOEXCEPT
{
    // May be on either network or notification strand (thread safe).

    const auto& query = archive();
    const auto out = query.get_tx_history(prevout.hash());
    if (!out.tx.is_valid())
        return false;

    status = { { "height", to_unsigned(out.tx.height()) } };
    if (const auto ins = query.get_spenders_history(prevout); !ins.empty())
    {
        status["spender_txhash"] = encode_hash(ins.front().tx.hash());
        status["spender_height"] = to_unsigned(ins.front().tx.height());
    }

    return true;
}

bool protocol_electrum::send_outpoint_status(const point& prevout,
    const std::string& hint) NOEXCEPT
{
    BC_ASSERT(stranded());

    // This is parsed for correctness but is not used.
    // Script is advisory, and should match output script.
    if (!hint.empty())
    {
        data_chunk bytes{};
        if (!decode_base16(bytes, hint))
        {
            send_code(error::invalid_argument);
            return false;
        }

        chain::script script{ std::move(bytes), false };
        if (!script.is_valid())
        {
            send_code(error::invalid_argument);
            return false;
        }
    }

    object_t status{};
    if (!get_outpoint_status(status, prevout))
    {
        send_code(error::not_found);
        return false;
    }

    send_result(std::move(status), 128, BIND(complete, _1));
    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
