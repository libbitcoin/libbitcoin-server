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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Utility methods.
// ----------------------------------------------------------------------------

static std::string to_script_type(chain::script_pattern pattern) NOEXCEPT
{
    using namespace chain;
    switch (pattern)
    {
        case script_pattern::pay_key_hash:
            return "pubkeyhash";
        case script_pattern::pay_script_hash:
            return "scripthash";
        case script_pattern::pay_multisig:
            return "multisig";
        case script_pattern::pay_public_key:
            return "pubkey";
        case script_pattern::pay_null_data:
            return "nulldata";
        case script_pattern::pay_witness_key_hash:
            return "witness_v0_keyhash";
        case script_pattern::pay_witness_script_hash:
            return "witness_v0_scripthash";
        case script_pattern::pay_witness_v1_taproot:
            return "witness_v1_taproot";
        default:
            return "nonstandard";
    }
}

bool protocol_bitcoind_rpc::handle_decode_script(const code& ec,
    rpc_interface::decode_script, const std::string& hex) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hex))
    {
        send_error(error::invalid_argument);
        return true;
    }

    using namespace chain;
    constexpr auto prefix = false;
    const script script{ data, prefix };
    if (!script.is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    using namespace wallet;
    const auto pattern = script.output_pattern();

    object_t result{};
    result.emplace("asm", script.to_string(flags::all_rules, true));
    result.emplace("type", to_script_type(pattern));

    if (pattern == script_pattern::pay_key_hash ||
        pattern == script_pattern::pay_script_hash)
    {
        const auto pay = payment_address::extract_output(script, p2kh_, p2sh_);
        if (pay)
            result.emplace("address", pay.encoded());
    }

    const payment_address pay{ script, p2sh_ };
    if (pay)
        result.emplace("p2sh", pay.encoded());

    send_result(std::move(result), 256);
    return true;
}

bool protocol_bitcoind_rpc::handle_validate_address(const code& ec,
    rpc_interface::validate_address, const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return false;

    using namespace wallet;
    const payment_address base58(address);
    if (base58)
    {
        send_result(object_t
        {
            { "isvalid", true },
            { "address", base58.encoded() },
            { "isscript", base58.prefix() == p2sh_ },
            { "iswitness", false }
        }, 128);
        return true;
    }

    const witness_address witness(address);
    if (witness && witness.prefix() == witness_)
    {
        const auto version0_p2sh = (witness.identifier() ==
            witness_address::program_type::version0_p2sh);

        send_result(object_t
        {
            { "isvalid", true },
            { "address", witness.encoded() },
            { "isscript", version0_p2sh },
            { "iswitness", true },
            { "witness_version", witness.version() },
            { "witness_program", encode_base16(witness.program()) }
        }, 128);
        return true;
    }

    send_result(object_t{ { "isvalid", false } }, 32);
    return true;
}

bool protocol_bitcoind_rpc::handle_create_multisig(const code& ec,
    rpc_interface::create_multisig) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_derive_addresses(const code& ec,
    rpc_interface::derive_addresses) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_descriptor_info(const code& ec,
    rpc_interface::get_descriptor_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_verify_message(const code& ec,
    rpc_interface::verify_message) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_index_info(const code& ec,
    rpc_interface::get_index_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_estimate_smart_fee(const code& ec,
    rpc_interface::estimate_smart_fee) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
