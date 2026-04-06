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
using namespace system::wallet;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

void protocol_electrum::handle_blockchain_utxo_get_address(const code& ec,
    rpc_interface::blockchain_utxo_get_address, const std::string& tx_hash,
    double index) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t offset{};
    hash_digest hash{};
    if (!to_integer(offset, index) ||
        !decode_hash(hash, tx_hash))
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

void protocol_electrum::handle_blockchain_address_get_balance(const code& ec,
    rpc_interface::blockchain_address_get_balance,
    const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    get_balance(extract_scripthash(address));
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

    // TODO: collect the address into a limited notification set.
    subscribed_address_.store(true, std::memory_order_relaxed);

    // TODO: compute the status hash in a store query (no mempool).
    send_result(array_t{ "status-hash" }, 16, BIND(complete, _1));
}

// utilities
// ----------------------------------------------------------------------------

hash_digest protocol_electrum::extract_scripthash(
    const std::string& address) const NOEXCEPT
{
    // TODO: capture failure condition (don't hash empty script).
    return payment_address(address).output_script(p2kh_, p2sh_).hash();
}

payment_address protocol_electrum::extract_address(
    const chain::script& script) const NOEXCEPT
{
    return payment_address::extract_output(script, p2kh_, p2sh_);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
