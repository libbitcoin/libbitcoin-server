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
#include <memory>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

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

// Utilities.
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

// Handlers (getters).
// ----------------------------------------------------------------------------

// Required by btcwallet during wallet chain-sync bootstrap.
bool protocol_btcd::handle_get_best_block(const code& ec,
    btcd_interface::get_best_block) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    send_result(object_t
    {
        { "hash", encode_hash(query.get_top_confirmed_hash()) },
        { "height", query.get_top_confirmed() }
    }, 96);
    return true;
}

// p2p magic (checked once by btcwallet/lnd to confirm network).
bool protocol_btcd::handle_get_current_net(const code& ec,
    btcd_interface::get_current_net) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(network_settings().identifier, 20);
    return true;
}

bool protocol_btcd::handle_get_difficulty(const code& ec,
    btcd_interface::get_difficulty) NOEXCEPT
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

    // btcd's numeric version encoding (1'000'000 major, 10'000 minor, 100 patch).
    const auto& segments = server_settings().btcd.version.segments();
    const auto version = 1'000'000 * segments[0] + 10'000 * segments[1] +
        100 * segments[2];

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

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto target = is_negative(height) ? top :
        std::min(sign_cast<size_t>(height), top);

    const auto header = query.get_header(query.to_confirmed(target));
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    const auto period = system_settings().block_spacing_seconds;
    const auto span = to_floating(power2<uint64_t>(32u));
    send_result(header->difficulty() * span / period, 20);
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

    using namespace chain;
    uint32_t vout{};
    hash_digest hash{};

    // The transaction owns shared inputs/outputs, so these are populated
    // directly, avoiding the intermediate vectors that to_shareds() copies.
    const auto ins = std::make_shared<input_cptrs>();
    ins->reserve(inputs.size());

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

        if (!decode_hash(hash, std::get<string_t>(txid_it->second.value())) ||
            !to_integer(vout, std::get<number_t>(vout_it->second.value())))
        {
            send_error(error::invalid_argument);
            return true;
        }

        ins->push_back(to_shared<input>(point{ hash, vout }, script{},
            max_input_sequence));
    }

    script script{};
    uint64_t satoshi{};
    const auto outs = std::make_shared<output_cptrs>();
    outs->reserve(outputs.size());
    for (const auto& pair: outputs)
    {
        if (!std::holds_alternative<number_t>(pair.second.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        if (const auto fault = btcd::output_script(script, pair.first, p2kh_,
            p2sh_, witness_))
        {
            send_error(fault);
            return true;
        }

        const auto btc = std::get<number_t>(pair.second.value());
        if (!to_integer(satoshi, btc * satoshi_per_bitcoin, false))
        {
            send_error(error::invalid_argument);
            return true;
        }

        outs->push_back(to_shared<output>(satoshi, std::move(script)));
    }

    constexpr auto witness = false;
    const transaction tx{ 1, ins, outs, locktime };
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

    send_result(value_from(bitcoind(tx)), two * tx.serialized_size(witness));
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

bool protocol_btcd::handle_validate_address(const code& ec,
    btcd_interface::validate_address, const std::string& address) NOEXCEPT
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

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
