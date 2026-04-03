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

void protocol_electrum::handle_blockchain_utxo_get_address(const code& ec,
    rpc_interface::blockchain_utxo_get_address,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: get the output by tx_hash and index.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_address_get_balance(const code& ec,
    rpc_interface::blockchain_address_get_balance,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: get by p2sh/p2pkh address.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_address_get_history(const code& ec,
    rpc_interface::blockchain_address_get_history,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: get by p2sh/p2pkh address.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_address_get_mempool(const code& ec,
    rpc_interface::blockchain_address_get_mempool,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: get by p2sh/p2pkh address.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_address_list_unspent(const code& ec,
    rpc_interface::blockchain_address_list_unspent,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: get by p2sh/p2pkh address.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_address_subscribe(const code& ec,
    rpc_interface::blockchain_address_subscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: get by p2sh/p2pkh address.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_balance(const code& ec,
    rpc_interface::blockchain_scripthash_get_balance,
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

    send_code(error::not_implemented);
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

    send_code(error::not_implemented);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
