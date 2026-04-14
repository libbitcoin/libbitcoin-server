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

#include <ranges>
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
    decode_hash(hash, scripthash);
    get_balance(hash);
}

void protocol_electrum::get_balance(const hash_digest& hash) NOEXCEPT
{
    if (hash == null_hash)
    {
        send_code(error::invalid_argument);
        return;
    }

    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    monitor(true);
    PARALLEL(do_get_balance, hash);
}

void protocol_electrum::do_get_balance(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    uint64_t confirmed{}, unconfirmed{};
    auto ec = query.get_balance(stopping_, confirmed, unconfirmed, hash);
    POST(complete_get_balance, ec, confirmed, unconfirmed);
}

void protocol_electrum::complete_get_balance(const code& ec,
    uint64_t confirmed, int64_t unconfirmed) NOEXCEPT
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

    send_result(object_t
    {
        { "confirmed", confirmed },
        { "unconfirmed", unconfirmed }
    }, 42, BIND(complete, _1));
}

// get_history
// ----------------------------------------------------------------------------
// undocumented change in v1.6 (mempool canonical ordering, always ordered).

void protocol_electrum::handle_blockchain_scripthash_get_history(const code& ec,
    rpc_interface::blockchain_scripthash_get_history,
    const std::string& scripthash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    decode_hash(hash, scripthash);
    get_history(hash);
}

void protocol_electrum::get_history(const system::hash_digest& hash) NOEXCEPT
{
    if (hash == null_hash)
    {
        send_code(error::invalid_argument);
        return;
    }

    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    monitor(true);
    PARALLEL(do_get_history, hash);
}

void protocol_electrum::do_get_history(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    database::histories histories{};
    const auto ec = query.get_history(stopping_, histories, hash, turbo_);
    POST(complete_get_history, ec, std::move(histories));
}

void protocol_electrum::complete_get_history(const code& ec,
    const database::histories& histories) NOEXCEPT
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

    const auto size = add1(histories.size()) * 128u;
    send_result(transform(histories), size, BIND(complete, _1));
}

// get_mempool
// ----------------------------------------------------------------------------
// undocumented change in v1.6 (mempool canonical ordering, always ordered).

void protocol_electrum::handle_blockchain_scripthash_get_mempool(const code& ec,
    rpc_interface::blockchain_scripthash_get_mempool,
    const std::string& scripthash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    decode_hash(hash, scripthash);
    get_mempool(hash);
}

void protocol_electrum::get_mempool(const system::hash_digest& hash) NOEXCEPT
{
    if (hash == null_hash)
    {
        send_code(error::invalid_argument);
        return;
    }

    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    monitor(true);
    PARALLEL(do_get_mempool, hash);
}

void protocol_electrum::do_get_mempool(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    database::histories histories{};
    auto ec = query.get_unconfirmed_history(stopping_, histories, hash, turbo_);
    POST(complete_get_mempool, ec, std::move(histories));
}

void protocol_electrum::complete_get_mempool(const code& ec,
    const database::histories& histories) NOEXCEPT
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

    const auto size = add1(histories.size()) * 128u;
    send_result(transform(histories), size, BIND(complete, _1));
}

// list_unspent
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_scripthash_list_unspent(const code& ec,
    rpc_interface::blockchain_scripthash_list_unspent,
    const std::string& scripthash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    decode_hash(hash, scripthash);
    list_unspent(hash);
}

void protocol_electrum::list_unspent(const system::hash_digest& hash) NOEXCEPT
{
    if (hash == null_hash)
    {
        send_code(error::invalid_argument);
        return;
    }

    if (!archive().address_enabled())
    {
        send_code(error::not_implemented);
        return;
    }

    monitor(true);
    PARALLEL(do_list_unspent, hash);
}

void protocol_electrum::do_list_unspent(const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    database::unspents unspents{};
    const auto ec = query.get_unspent(stopping_, unspents, hash, turbo_);
    POST(complete_list_unspent, ec, std::move(unspents));
}

void protocol_electrum::complete_list_unspent(const code& ec,
    const database::unspents& unspents) NOEXCEPT
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

    const auto size = add1(unspents.size()) * 128u;
    send_result(transform(unspents), size, BIND(complete, _1));
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
    send_result(array_t{ string_t{ "status-hash" } }, 16, BIND(complete, _1));
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

    // TODO: remove scripthash subscription from the notification set.
    const auto previous = subscribed_scriptpubkey_.load(
        std::memory_order_relaxed);

    send_result(previous, 16, BIND(complete, _1));
}

// utilities
// ----------------------------------------------------------------------------
// TODO: these can be implemented as electrum json serializers (see bitcoind).

// Height is set to 0 for unconfirmed tx fully chain rooted and -1 otherwise.
array_t protocol_electrum::transform(const database::histories& ins) NOEXCEPT
{
    // Height is set to zero or max_size_t for unconfirmed history.
    // to_signed() conversion is simple but sacrifices top height bit (ok).
    static_assert(to_signed(max_size_t) == -1 && is_max(max_size_t));

    array_t out(ins.size());
    std::ranges::transform(ins, out.begin(), [](const auto& in) NOEXCEPT
    {
        object_t object
        {
            { "height", to_signed(in.tx.height()) },
            { "tx_hash", encode_hash(in.tx.hash()) }
        };

        if (!in.confirmed())
        {
            // A fee of max_uint64 implies missing prevout(s). This will happen
            // for a block-downloaded tx queried during parallel block download
            // when the prevout block(s) are not yet archived or even if the tx
            // turned out to be in an invalid block after its block download. A
            // transaction is technically never invalid absent a block context.
            object["fee"] = in.fee;
        }

        return object;
    });

    return out;
}

// Height is set to 0 for unconfirmed unspent output txs.
array_t protocol_electrum::transform(const database::unspents& ins) NOEXCEPT
{
    array_t out(ins.size());
    std::ranges::transform(ins, out.begin(), [](const auto& in) NOEXCEPT
    {
        const auto& out = in.out;
        return object_t
        {
            { "tx_hash", encode_hash(out.point().hash()) },
            { "tx_pos",  out.point().index() },
            { "height",  in.height },
            { "value",   out.value() }
        };
    });

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
