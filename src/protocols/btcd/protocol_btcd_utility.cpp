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
#include <bitcoin/server/protocols/protocol_btcd.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Handlers (getters).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_get_best_block(const code& ec,
    btcd_interface::get_best_block) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Required by btcwallet during wallet chain-sync bootstrap.
    const auto& query = archive();
    object_t result{};
    result.emplace("hash", encode_hash(query.get_top_confirmed_hash()));
    result.emplace("height", query.get_top_confirmed());
    send_result(std::move(result), 96);
    return true;
}

bool protocol_btcd::handle_get_current_net(const code& ec,
    btcd_interface::get_current_net) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The p2p handshake magic, the same value btcd returns (checked once by
    // btcwallet/lnd at connect to confirm the expected network).
    send_result(network_settings().identifier, 20);
    return true;
}

bool protocol_btcd::handle_get_difficulty(const code& ec,
    btcd_interface::get_difficulty) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto header = query.get_header(query.to_confirmed(
        query.get_top_confirmed()));
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    send_result(header->difficulty(), 20);
    return true;
}

bool protocol_btcd::handle_get_info(const code& ec,
    btcd_interface::get_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    // btcd's numeric version encoding (1'000'000 major, 10'000 minor, 100
    // patch).
    const auto& segments = server_settings().btcd.version.segments();
    const auto version = 1'000'000 * segments[0] + 10'000 * segments[1] +
        100 * segments[2];

    // Untracked peer-dependent fields are reported as empty/zero, matching
    // the inherited getnetworkinfo handler's convention.
    send_result(object_t
    {
        { "version", version },
        { "protocolversion", network_settings().protocol_maximum },
        { "blocks", top },
        { "timeoffset", 0 },
        { "connections", 0 },
        { "proxy", std::string{} },
        { "difficulty", header->difficulty() },
        { "testnet", chain_name(query) != "main" },
        { "relayfee", node_settings().minimum_fee_rate },
        { "errors", std::string{} }
    }, 256);
    return true;
}

bool protocol_btcd::handle_get_net_totals(const code& ec,
    btcd_interface::get_net_totals) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Untracked byte counters are reported as zero; timemillis is real.
    send_result(object_t
    {
        { "totalbytesrecv", 0 },
        { "totalbytessent", 0 },
        { "timemillis", possible_wide_cast<int64_t>(zulu_time()) * 1'000 }
    }, 64);
    return true;
}

bool protocol_btcd::handle_get_network_hash_ps(const code& ec,
    btcd_interface::get_network_hash_ps, uint32_t, int32_t height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Approximated from the requested height's difficulty and the configured
    // target spacing, not a windowed average over 'blocks'.
    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto target = is_negative(height) ? top :
        std::min(possible_sign_cast<size_t>(height), top);

    const auto header = query.get_header(query.to_confirmed(target));
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    constexpr auto two_to_the_32 = 4294967296.0;
    const auto spacing = system_settings().block_spacing_seconds;
    send_result(header->difficulty() * two_to_the_32 / spacing, 20);
    return true;
}

// Handlers (tools).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_create_raw_transaction(const code& ec,
    btcd_interface::create_raw_transaction, const array_t& inputs,
    const object_t& outputs, uint32_t locktime) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::inputs ins{};
    ins.reserve(inputs.size());
    for (const auto& item: inputs)
    {
        if (!std::holds_alternative<object_t>(item.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        const auto& fields = std::get<object_t>(item.value());
        const auto txid_it = fields.find("txid");
        const auto vout_it = fields.find("vout");
        if (txid_it == fields.end() || vout_it == fields.end() ||
            !std::holds_alternative<string_t>(txid_it->second.value()) ||
            !std::holds_alternative<number_t>(vout_it->second.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        hash_digest hash{};
        uint32_t vout{};
        if (!decode_hash(hash, std::get<string_t>(txid_it->second.value())) ||
            !to_integer(vout, std::get<number_t>(vout_it->second.value())))
        {
            send_error(error::invalid_argument);
            return true;
        }

        ins.emplace_back(chain::point{ hash, vout }, chain::script{},
            0xffffffff_u32);
    }

    chain::outputs outs{};
    outs.reserve(outputs.size());
    for (const auto& pair: outputs)
    {
        if (!std::holds_alternative<number_t>(pair.second.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        chain::script script{};
        if (const auto fault = parse_output_script(pair.first, script))
        {
            send_error(fault);
            return true;
        }

        // Scale to satoshis and validate the domain (not whole, as scaling
        // carries floating point representation error).
        uint64_t satoshi{};
        const auto btc = std::get<number_t>(pair.second.value());
        if (!to_integer(satoshi, btc * chain::satoshi_per_bitcoin, false))
        {
            send_error(error::invalid_argument);
            return true;
        }

        outs.emplace_back(satoshi, std::move(script));
    }

    const chain::transaction tx{ 1, std::move(ins), std::move(outs),
        locktime };

    // Unsigned, so no witness data yet -- plain (non-witness) serialization.
    constexpr auto witness = false;
    send_result(to_text(tx, tx.serialized_size(witness), witness), 400);
    return true;
}

bool protocol_btcd::handle_decode_raw_transaction(const code& ec,
    btcd_interface::decode_raw_transaction,
    const std::string& hexstring) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::invalid_argument);
        return true;
    }

    constexpr auto witness = true;
    const chain::transaction tx{ data, witness };
    if (!tx.is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    // bitcoind(tx) alone (no inject_tx_context) is the bare
    // decoderawtransaction shape -- a standalone tx has no block context.
    send_result(value_from(bitcoind(tx)),
        two * tx.serialized_size(witness));
    return true;
}

bool protocol_btcd::handle_decode_script(const code& ec,
    btcd_interface::decode_script, const std::string& hex) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hex))
    {
        send_error(error::invalid_argument);
        return true;
    }

    // false: raw script bytes, no serialized length prefix.
    const chain::script script{ data, false };
    if (!script.is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    using namespace chain;
    object_t result{};
    result.emplace("asm", script.to_string(flags::all_rules, true));

    std::string kind{ "nonstandard" };
    switch (script.output_pattern())
    {
        case script_pattern::pay_key_hash:
            kind = "pubkeyhash";
            break;
        case script_pattern::pay_script_hash:
            kind = "scripthash";
            break;
        case script_pattern::pay_multisig:
            kind = "multisig";
            break;
        case script_pattern::pay_public_key:
            kind = "pubkey";
            break;
        case script_pattern::pay_null_data:
            kind = "nulldata";
            break;
        case script_pattern::pay_witness_key_hash:
            kind = "witness_v0_keyhash";
            break;
        case script_pattern::pay_witness_script_hash:
            kind = "witness_v0_scripthash";
            break;
        case script_pattern::pay_witness_v1_taproot:
            kind = "witness_v1_taproot";
            break;
        default:
            break;
    }
    result.emplace("type", kind);

    if (script.output_pattern() == script_pattern::pay_key_hash ||
        script.output_pattern() == script_pattern::pay_script_hash)
    {
        const auto address = wallet::payment_address::extract_output(script,
            p2kh_, p2sh_);
        if (address)
            result.emplace("address", address.encoded());
    }

    // The p2sh-wrapping address of this exact script (btcd's "p2sh" field).
    const wallet::payment_address wrapped{ script, p2sh_ };
    if (wrapped)
        result.emplace("p2sh", wrapped.encoded());

    send_result(std::move(result), 256);
    return true;
}

bool protocol_btcd::handle_validate_address(const code& ec,
    btcd_interface::validate_address, const std::string& address) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const wallet::payment_address base58(address);
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

    const wallet::witness_address segwit(address);
    if (segwit && segwit.prefix() == witness_)
    {
        send_result(object_t
        {
            { "isvalid", true },
            { "address", segwit.encoded() },
            { "isscript", segwit.identifier() ==
                wallet::witness_address::program_type::version0_p2sh },
            { "iswitness", true },
            { "witness_version", segwit.version() },
            { "witness_program", encode_base16(segwit.program()) }
        }, 128);
        return true;
    }

    send_result(object_t{ { "isvalid", false } }, 32);
    return true;
}

// Utilities.
// ----------------------------------------------------------------------------

code protocol_btcd::parse_output_script(const std::string& text,
    chain::script& out) NOEXCEPT
{
    const wallet::payment_address base58(text);
    if (base58)
    {
        out = base58.output_script(p2kh_, p2sh_);
        return error::success;
    }

    // The parse accepts any prefix, so the configured one is a check.
    const wallet::witness_address segwit(text);
    if (segwit && segwit.prefix() == witness_)
    {
        out = segwit.script();
        return error::success;
    }

    return error::invalid_argument;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
