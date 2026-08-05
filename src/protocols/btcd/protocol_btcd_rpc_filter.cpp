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
#include <bitcoin/server/protocols/protocol_btcd_rpc.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd_rpc

using namespace system;
using namespace network;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Filter parsing (loadtxfilter).
// ----------------------------------------------------------------------------

code protocol_btcd_rpc::parse_filter_addresses(bool reload,
    const value_t& addresses) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (reload)
    {
        pay_key_hashes_.clear();
        pay_script_hashes_.clear();
        pay_witness_key_hash_programs_.clear();
        pay_witness_script_hash_programs_.clear();
        pay_witness_taproot_programs_.clear();
    }

    if (!std::holds_alternative<array_t>(addresses.value()))
        return error::invalid_argument;

    for (const auto& item: std::get<array_t>(addresses.value()))
    {
        if (!std::holds_alternative<string_t>(item.value()))
            return error::invalid_argument;

        const auto& text = std::get<string_t>(item.value());

        // Try base58 (p2kh/p2sh) first.
        const wallet::payment_address base58(text);
        if (base58)
        {
            if (base58.prefix() == p2kh_)
                pay_key_hashes_.insert(base58.hash());
            else if (base58.prefix() == p2sh_)
                pay_script_hashes_.insert(base58.hash());
            else
                return error::invalid_argument;

            continue;
        }

        // Otherwise bech32/bech32m (p2wpkh/p2wsh/p2tr).
        const wallet::witness_address segwit(text);
        if (!segwit)
            return error::invalid_argument;

        switch (segwit.identifier())
        {
            case wallet::witness_address::program_type::version0_p2kh:
                pay_witness_key_hash_programs_.insert(segwit.program());
                break;
            case wallet::witness_address::program_type::version0_p2sh:
                pay_witness_script_hash_programs_.insert(segwit.program());
                break;
            case wallet::witness_address::program_type::version1_taproot:
                pay_witness_taproot_programs_.insert(segwit.program());
                break;
            default:
                return error::invalid_argument;
        }
    }

    return error::success;
}

code protocol_btcd_rpc::parse_filter_outpoints(bool reload,
    const value_t& outpoints) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (reload)
        filter_outpoints_.clear();

    if (!std::holds_alternative<array_t>(outpoints.value()))
        return error::invalid_argument;

    for (const auto& item: std::get<array_t>(outpoints.value()))
    {
        if (!std::holds_alternative<object_t>(item.value()))
            return error::invalid_argument;

        const auto& fields = std::get<object_t>(item.value());
        const auto hash_it = fields.find("hash");
        const auto index_it = fields.find("index");
        if (hash_it == fields.end() || index_it == fields.end() ||
            !std::holds_alternative<string_t>(hash_it->second.value()) ||
            !std::holds_alternative<number_t>(index_it->second.value()))
            return error::invalid_argument;

        hash_digest hash{};
        uint32_t index{};
        if (!decode_hash(hash, std::get<string_t>(hash_it->second.value())) ||
            !to_integer(index, std::get<number_t>(index_it->second.value())))
            return error::invalid_argument;

        filter_outpoints_.emplace(hash, index);
    }

    return error::success;
}

// Handlers (address/outpoint filtering).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_load_tx_filter(const code& ec,
    btcd_interface::load_tx_filter, bool reload, const value_t& addresses,
    const value_t& outpoints) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (const auto fault = parse_filter_addresses(reload, addresses))
    {
        send_btcd_error(fault);
        return true;
    }

    if (const auto fault = parse_filter_outpoints(reload, outpoints))
    {
        send_btcd_error(fault);
        return true;
    }

    send_btcd_result({}, 4);
    return true;
}

bool protocol_btcd_rpc::handle_rescan_blocks(const code& ec,
    btcd_interface::rescan_blocks, const value_t& blockhashes) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!std::holds_alternative<array_t>(blockhashes.value()))
    {
        send_btcd_error(error::invalid_argument);
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    array_t discovered{};

    for (const auto& item: std::get<array_t>(blockhashes.value()))
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_btcd_error(error::invalid_argument);
            return true;
        }

        const auto& text = std::get<string_t>(item.value());
        hash_digest hash{};
        if (!decode_hash(hash, text))
        {
            send_btcd_error(error::invalid_argument);
            return true;
        }

        const auto block = query.get_block(query.to_header(hash), witness);
        if (!block)
        {
            send_btcd_error(error::not_found);
            return true;
        }

        auto matched = match_filtered_transactions(*block);
        if (!matched.empty())
        {
            discovered.emplace_back(value_t{ object_t
            {
                { "hash", value_t{ text } },
                { "transactions", value_t{ std::move(matched) } }
            } });
        }
    }

    send_btcd_result(value_t{ std::move(discovered) }, 256);
    return true;
}

// Filter matching.
// ----------------------------------------------------------------------------
// Manual per-block script inspection, not the persisted address index --
// that index is for whole-chain lookups, the wrong tool for matching one
// block against a small in-memory watch-list, and would require the index
// to be enabled. Mirrors btcd's per-client wsClientFilter.

array_t protocol_btcd_rpc::match_filtered_transactions(
    const chain::block& block) NOEXCEPT
{
    BC_ASSERT(stranded());
    constexpr auto witness = true;

    array_t matched{};
    for (const auto& tx: *block.transactions_ptr())
    {
        auto hit = false;

        for (const auto& input: *tx->inputs_ptr())
            if (filter_outpoints_.contains(input->point()))
                hit = true;

        const auto hash = tx->hash(false);
        const auto& outputs = *tx->outputs_ptr();
        for (size_t index = 0; index < outputs.size(); ++index)
        {
            const auto& script = outputs[index]->script();
            const auto point_index = possible_narrow_cast<uint32_t>(index);

            switch (script.output_pattern())
            {
                case chain::script_pattern::pay_key_hash:
                {
                    const auto address = wallet::payment_address::
                        extract_output(script, p2kh_, p2sh_);
                    if (address && pay_key_hashes_.contains(address.hash()))
                    {
                        hit = true;
                        filter_outpoints_.emplace(hash, point_index);
                    }
                    break;
                }
                case chain::script_pattern::pay_script_hash:
                {
                    const auto address = wallet::payment_address::
                        extract_output(script, p2kh_, p2sh_);
                    if (address && pay_script_hashes_.contains(address.hash()))
                    {
                        hit = true;
                        filter_outpoints_.emplace(hash, point_index);
                    }
                    break;
                }
                case chain::script_pattern::pay_witness_key_hash:
                {
                    const auto& program = script.witness_program();
                    if (program && pay_witness_key_hash_programs_.contains(
                        *program))
                    {
                        hit = true;
                        filter_outpoints_.emplace(hash, point_index);
                    }
                    break;
                }
                case chain::script_pattern::pay_witness_script_hash:
                {
                    const auto& program = script.witness_program();
                    if (program && pay_witness_script_hash_programs_.contains(
                        *program))
                    {
                        hit = true;
                        filter_outpoints_.emplace(hash, point_index);
                    }
                    break;
                }
                case chain::script_pattern::pay_witness_v1_taproot:
                {
                    const auto& program = script.witness_program();
                    if (program && pay_witness_taproot_programs_.contains(
                        *program))
                    {
                        hit = true;
                        filter_outpoints_.emplace(hash, point_index);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (hit)
            matched.emplace_back(value_t{ to_text(*tx,
                tx->serialized_size(witness), witness) });
    }

    return matched;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
