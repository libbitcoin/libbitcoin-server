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
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

void protocol_electrum::handle_blockchain_scriptpubkey_get_balance(
    const code& ec, rpc_interface::blockchain_scriptpubkey_get_balance,
    const std::string& scriptpubkey) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk bytes{};
    if (!decode_base16(bytes, scriptpubkey))
    {
        send_code(error::invalid_argument);
        return;
    }

    chain::script script{ std::move(bytes), false };
    if (!script.is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    get_balance(script.hash());
}

void protocol_electrum::handle_blockchain_scriptpubkey_get_history(
    const code& ec, rpc_interface::blockchain_scriptpubkey_get_history,
    const std::string& scriptpubkey) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk bytes{};
    if (!decode_base16(bytes, scriptpubkey))
    {
        send_code(error::invalid_argument);
        return;
    }

    chain::script script{ std::move(bytes), false };
    if (!script.is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    get_history(script.hash());
}

void protocol_electrum::handle_blockchain_scriptpubkey_get_mempool(
    const code& ec, rpc_interface::blockchain_scriptpubkey_get_mempool,
    const std::string& scriptpubkey) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk bytes{};
    if (!decode_base16(bytes, scriptpubkey))
    {
        send_code(error::invalid_argument);
        return;
    }

    chain::script script{ std::move(bytes), false };
    if (!script.is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    get_mempool(script.hash());
}

void protocol_electrum::handle_blockchain_scriptpubkey_list_unspent(
    const code& ec, rpc_interface::blockchain_scriptpubkey_list_unspent,
    const std::string& scriptpubkey) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk bytes{};
    if (!decode_base16(bytes, scriptpubkey))
    {
        send_code(error::invalid_argument);
        return;
    }

    chain::script script{ std::move(bytes), false };
    if (!script.is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    list_unspent(script.hash());
}

void protocol_electrum::handle_blockchain_scriptpubkey_subscribe(
    const code& ec, rpc_interface::blockchain_scriptpubkey_subscribe,
    const std::string& scriptpubkey) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk bytes{};
    if (!decode_base16(bytes, scriptpubkey))
    {
        send_code(error::invalid_argument);
        return;
    }

    chain::script script{ std::move(bytes), false };
    if (!script.is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    // TODO: collect the address into a limited notification set.
    subscribed_address_.store(true, std::memory_order_relaxed);

    // TODO: compute the status hash in a store query (no mempool).
    send_result(object_t{}, 16, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_scriptpubkey_unsubscribe(
    const code& ec, rpc_interface::blockchain_scriptpubkey_unsubscribe,
    const std::string& scriptpubkey) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk bytes{};
    if (!decode_base16(bytes, scriptpubkey))
    {
        send_code(error::invalid_argument);
        return;
    }

    chain::script script{ std::move(bytes), false };
    if (!script.is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    // TODO: remove script subscription from notification set.
    const auto previous = subscribed_scriptpubkey_.load(
        std::memory_order_relaxed);

    send_result(previous, 16, BIND(complete, _1));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
