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

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// NOTE: undocumented change in v1.6 (mempool txs have a canonical ordering).

// get_balance
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_get_balance(const code& ec,
    rpc_interface::blockchain_scripthash_get_balance,
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

    get_balance(hash);
}

void protocol_electrum::get_balance(const hash_digest& hash) NOEXCEPT
{
    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_balance, hash);
}

void protocol_electrum::do_get_balance(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    uint64_t confirmed{}, unconfirmed{};

    // TODO: add query to return both confirmed and unconfirmed balances.
    auto ec = query.get_confirmed_balance(stopping_, confirmed, hash, turbo_);

    POST(complete_get_balance, ec, confirmed, unconfirmed);
}

void protocol_electrum::complete_get_balance(const code& ec,
    uint64_t confirmed, uint64_t unconfirmed) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Stop monitoring socket.
    monitor(false);

    // Suppresses cancelation error response.
    if (stopped())
        return;

    if (ec)
    {
        send_code(error::server_error);
        return;
    }

    send_result(object_t
    {
        { "confirmed", confirmed },
        { "unconfirmed", unconfirmed }
    }, 42, BIND(complete, _1));
}

// get_history
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_get_history(const code& ec,
    rpc_interface::blockchain_scripthash_get_history,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

// get_mempool
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_get_mempool(const code& ec,
    rpc_interface::blockchain_scripthash_get_mempool,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    ////const auto sort = at_least(electrum::version::v1_6);

    send_code(error::not_implemented);
}

// list_unspent
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_list_unspent(const code& ec,
    rpc_interface::blockchain_scripthash_list_unspent,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

// subscribe/unsubscribe
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_subscribe(const code& ec,
    rpc_interface::blockchain_scripthash_subscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: collect the scripthash into a limited notification set.
    subscribed_scripthash_.store(true, std::memory_order_relaxed);

    // TODO: compute the status hash in a store query (no mempool).
    send_result(array_t{ "status-hash" }, 16, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_scripthash_unsubscribe(const code& ec,
    rpc_interface::blockchain_scripthash_unsubscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4_2))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: remove the scripthash from the notification set.
    subscribed_scripthash_.store(false, std::memory_order_relaxed);

    // TODO: return false if the scripthash was not found.
    send_result(true, 16, BIND(complete, _1));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
