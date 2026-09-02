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
#include <bitcoin/server/protocols/protocol_bitcoind_utility.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_utility
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_utility::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_decode_script, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_validate_address, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_create_multisig, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_derive_addresses, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_descriptor_info, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_verify_message, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_index_info, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_estimate_smart_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sign_message_with_priv_key, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

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

bool protocol_bitcoind_utility::handle_decode_script(const code& ec,
    rpc_interface::decode_script, const std::string& hexstring) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    using namespace chain;
    constexpr auto prefix = false;
    const script script{ data, prefix };

    using namespace wallet;
    const auto pattern = script.output_pattern();
    object_t result
    {
        { "asm", script.to_string(flags::all_rules, true) },
        { "desc", infer_descriptor(script, p2kh_, p2sh_, witness_) },
        { "type", to_script_type(pattern) }
    };

    // An undecodable script is rendered, not rejected.
    if (!script.is_valid() || script.is_underflow())
    {
        const auto body = "raw(" + encode_base16(data) + ")";
        result["desc"] = body + "#" + descriptor_checksum(body);
        send_result(std::move(result), 512);
        return true;
    }

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

    // Witness-embeddable scripts carry the version 0 program forms.
    if (!chain::script::is_pay_witness_pattern(script.ops()) &&
        !chain::script::is_pay_null_data_pattern(script.ops()))
    {
        const chain::script wsh{ chain::script::to_pay_witness_pattern(0,
            sha256_hash(script.to_data(false))) };

        result.emplace("segwit", object_t
        {
            { "asm", wsh.to_string(flags::all_rules, true) },
            { "hex", encode_base16(wsh.to_data(false)) },
            { "type", to_script_type(script_pattern::pay_witness_script_hash) },
            { "address", witness_address{ script, witness_ }.encoded() },
            { "desc", infer_descriptor(wsh, p2kh_, p2sh_, witness_) },
            { "p2sh-segwit", payment_address{ wsh, p2sh_ }.encoded() }
        });
    }

    send_result(std::move(result), 512);
    return true;
}

bool protocol_bitcoind_utility::handle_validate_address(const code& ec,
    rpc_interface::validate_address, const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return false;

    using namespace wallet;
    const payment_address base58(address);
    if (base58 && ((base58.prefix() == p2kh_) || (base58.prefix() == p2sh_)))
    {
        send_result(object_t
        {
            { "isvalid", true },
            { "address", base58.encoded() },
            { "scriptPubKey", encode_base16(base58.output_script(p2kh_,
                p2sh_).to_data(false)) },
            { "isscript", base58.prefix() == p2sh_ },
            { "iswitness", false }
        }, 128);
        return true;
    }

    const witness_address witness{ address };
    if (witness && witness.prefix() == witness_)
    {
        const auto version0_p2sh = (witness.identifier() ==
            witness_address::program_type::version0_p2sh);

        send_result(object_t
        {
            { "isvalid", true },
            { "address", witness.encoded() },
            { "scriptPubKey", encode_base16(witness.script().to_data(false)) },
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

// bech32m multisig is rejected.
bool protocol_bitcoind_utility::handle_create_multisig(const code& ec,
    rpc_interface::create_multisig, double nrequired, const array_t& keys,
    const std::string& address_type) NOEXCEPT
{
    if (stopped(ec))
        return false;

    uint8_t required{};
    if (!to_integer(required, nrequired) || is_zero(required) ||
        required > keys.size())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    // The multisig pattern is limited to op_16 (bitcoind allows 20 for wsh).
    constexpr auto maximum_keys = 16_size;
    if (keys.size() > maximum_keys)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    if (address_type != "legacy" && 
        address_type != "p2sh-segwit" &&
        address_type != "bech32")
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    // An invalid key (bitcoind -5) and an oversized script (-8) are not
    // distinguished by the helper.
    auto result = create_multisig(required, keys, address_type, p2sh_,
        witness_);
    if (result.empty())
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    send_result(std::move(result), 256);
    return true;
}

bool protocol_bitcoind_utility::handle_derive_addresses(const code& ec,
    rpc_interface::derive_addresses, const std::string& expression,
    const std::optional<value_t>& range) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const wallet::descriptor parsed{ expression };
    if (!parsed)
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    if (parsed.ranged() != range.has_value())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    // The range is an end index or a [begin, end] pair.
    uint32_t begin{};
    uint32_t end{};
    if (range.has_value())
    {
        const auto& value = range.value().value();
        if (std::holds_alternative<number_t>(value))
        {
            if (!to_integer(end, std::get<number_t>(value)))
            {
                send_error(error::bitcoind::invalid_parameter);
                return true;
            }
        }
        else if (std::holds_alternative<array_t>(value))
        {
            const auto& pair = std::get<array_t>(value);
            if (pair.size() != two ||
                !std::holds_alternative<number_t>(pair.front().value()) ||
                !std::holds_alternative<number_t>(pair.back().value()) ||
                !to_integer(begin, std::get<number_t>(pair.front().value())) ||
                !to_integer(end, std::get<number_t>(pair.back().value())) ||
                end < begin)
            {
                send_error(error::bitcoind::invalid_parameter);
                return true;
            }
        }
        else
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }
    }

    // bitcoind's derivation range limit.
    constexpr uint32_t maximum_range = 10'000;
    if (floored_subtract(end, begin) >= maximum_range)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    array_t out{};
    for (auto index = begin; index <= end; ++index)
    {
        const auto scripts = parsed.scripts(index);
        std::string address{};
        if (is_one(scripts.size()))
            address = to_address(scripts.front(), p2kh_, p2sh_, witness_);

        if (address.empty())
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }

        out.emplace_back(std::move(address));
    }

    const auto size = 64 * out.size();
    send_result(std::move(out), size);
    return true;
}

bool protocol_bitcoind_utility::handle_get_descriptor_info(const code& ec,
    rpc_interface::get_descriptor_info,
    const std::string& expression) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const wallet::descriptor parsed{ expression };
    if (!parsed)
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    send_result(object_t
    {
        { "descriptor", parsed.encoded() },
        { "checksum", parsed.checksum() },
        { "isrange", parsed.ranged() },
        { "issolvable", parsed.solvable() },
        { "hasprivatekeys", parsed.has_private_keys() }
    }, 256);
    return true;
}

bool protocol_bitcoind_utility::handle_verify_message(const code& ec,
    rpc_interface::verify_message, const std::string& address,
    const std::string& signature, const std::string& message) NOEXCEPT
{
    if (stopped(ec))
        return false;

    using namespace wallet;
    const payment_address payment(address);
    if (!payment)
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    data_chunk decoded{};
    if (!decode_base64(decoded, signature) ||
        decoded.size() != message_signature_size)
    {
        send_error(error::bitcoind::type_error);
        return true;
    }

    message_signature signature_bytes{};
    std::copy_n(decoded.begin(), signature_bytes.size(),
        signature_bytes.begin());

    const auto verified = verify_message(message, payment, signature_bytes);
    send_result(value{ verified }, 8);
    return true;
}

bool protocol_bitcoind_utility::handle_get_index_info(const code& ec,
    rpc_interface::get_index_info, const std::string& index_name) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Indexes track the confirmed chain only; all txs are archived (lookup).
    // synced: current chain and confirmation coalesced with the candidate top.
    const auto& query = archive();
    const object_t status
    {
        { "synced", is_current_chain(true) && query.is_coalesced() },
        { "best_block_height", query.get_top_confirmed() }
    };

    object_t result{};
    if (index_name.empty() || index_name == "txindex")
        result.emplace("txindex", status);

    if (query.filter_enabled() &&
        (index_name.empty() || index_name == "basic block filter index"))
        result.emplace("basic block filter index", status);

    send_result(std::move(result), 128);
    return true;
}

bool protocol_bitcoind_utility::handle_estimate_smart_fee(const code& ec,
    rpc_interface::estimate_smart_fee) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::internal_error);
    return true;
}

// Signing is a wallet function, keys never transit the server.
bool protocol_bitcoind_utility::handle_sign_message_with_priv_key(
    const code& ec, rpc_interface::sign_message_with_priv_key) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
