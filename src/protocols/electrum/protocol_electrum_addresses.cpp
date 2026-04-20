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
    const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    get_history(extract_scripthash(address));
}

void protocol_electrum::handle_blockchain_address_get_mempool(const code& ec,
    rpc_interface::blockchain_address_get_mempool,
    const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    get_mempool(extract_scripthash(address));
}

void protocol_electrum::handle_blockchain_address_list_unspent(const code& ec,
    rpc_interface::blockchain_address_list_unspent,
    const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    list_unspent(extract_scripthash(address));
}

void protocol_electrum::handle_blockchain_address_subscribe(const code& ec,
    rpc_interface::blockchain_address_subscribe,
    const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    const auto hash = extract_scripthash(address);
    if (hash == null_hash)
    {
        send_code(error::invalid_argument);
        return;
    }

    scripthash_subscribe(hash, notify_t::address);
}

// utilities
// ----------------------------------------------------------------------------
// A p2pk output does not produce a bitcoin payment address, as there is no
// form of a p2pk bitcoin payment address.

// A failed parse or default initialized script returns null_hash.
hash_digest protocol_electrum::extract_scripthash(
    const std::string& address) const NOEXCEPT
{
    const auto payment = payment_address(address);
    if (!payment)
        return {};

    const auto script = payment.output_script(p2kh_, p2sh_);
    if (!script.is_valid())
        return {};

    return script.hash();
}

// A failure to parse as p2sh or p2kh returns invalid object.
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
