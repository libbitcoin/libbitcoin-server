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
#include <bitcoin/server/parsers/bitcoind_transaction.hpp>

#include <utility>
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/bitcoind_script.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace system::chain;
using namespace network;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

code parse_transaction(transaction& out, const array_t& inputs,
    const value_t& outputs, double locktime, bool replaceable, double version,
    uint8_t p2kh, uint8_t p2sh, const std::string& witness,
    uint64_t maximum) NOEXCEPT
{
    uint32_t lock_time{};
    if (!to_integer(lock_time, locktime))
        return error::bitcoind::invalid_parameter;

    // bitcoind bounds the version to the maximum standard (currently 3).
    uint32_t tx_version{};
    if (!to_integer(tx_version, version) || is_zero(tx_version) ||
        (tx_version > 3u))
        return error::bitcoind::invalid_parameter;

    const auto sequence = replaceable ? messages::peer::bip125_sequence :
        (is_zero(lock_time) ? max_input_sequence : sub1(max_input_sequence));

    const auto ins = to_shared<input_cptrs>();
    ins->reserve(inputs.size());
    hash_digest hash{};
    uint32_t vout{};

    for (const auto& item: inputs)
    {
        if (!std::holds_alternative<object_t>(item.value()))
            return error::bitcoind::type_error;

        const auto& fields = std::get<object_t>(item.value());
        const auto txid_it = fields.find("txid");
        const auto vout_it = fields.find("vout");
        if (txid_it == fields.end() || vout_it == fields.end() ||
            !std::holds_alternative<string_t>(txid_it->second.value()) ||
            !std::holds_alternative<number_t>(vout_it->second.value()))
            return error::bitcoind::invalid_parameter;

        if (!decode_hash(hash, std::get<string_t>(txid_it->second.value())) ||
            !to_integer(vout, std::get<number_t>(vout_it->second.value())))
            return error::bitcoind::invalid_parameter;

        // An explicit sequence overrides the derived default.
        auto sequenced = sequence;
        const auto sequence_it = fields.find("sequence");
        if (sequence_it != fields.end() &&
            (!std::holds_alternative<number_t>(sequence_it->second.value()) ||
            !to_integer(sequenced,
                std::get<number_t>(sequence_it->second.value()))))
            return error::bitcoind::invalid_parameter;

        ins->push_back(to_shared<input>(point{ hash, vout }, script{},
            sequenced));
    }

    const auto outs = std::make_shared<output_cptrs>();

    // Appends one address or data output from a name/value pair.
    const auto append = [&](const std::string& name,
        const value_t& item) NOEXCEPT -> code
    {
        // A data output carries a null data script and no value.
        if (name == "data")
        {
            data_chunk data{};
            if (!std::holds_alternative<string_t>(item.value()) ||
                !decode_base16(data, std::get<string_t>(item.value())) ||
                data.size() > max_null_data_size)
                return error::bitcoind::invalid_parameter;

            outs->push_back(to_shared<output>(zero,
                chain::script{ script::to_pay_null_data_pattern(data) }));
            return error::bitcoind::success;
        }

        script script{};
        if (output_script(script, name, p2kh, p2sh, witness))
            return error::bitcoind::invalid_address_or_key;

        // bitcoind also accepts a quoted amount (slop; not special-cased).
        if (!std::holds_alternative<number_t>(item.value()))
            return error::bitcoind::type_error;

        uint64_t satoshis{};
        const auto bitcoins = std::get<number_t>(item.value());
        if (!to_integer(satoshis, bitcoins * satoshi_per_bitcoin, true) ||
            satoshis > maximum)
            return error::bitcoind::type_error;

        outs->push_back(to_shared<output>(satoshis, std::move(script)));
        return error::bitcoind::success;
    };

    // outputs is one object or an array of objects (permits repeated address).
    if (std::holds_alternative<object_t>(outputs.value()))
    {
        for (const auto& pair: std::get<object_t>(outputs.value()))
            if (const auto fault = append(pair.first, pair.second))
                return fault;
    }
    else if (std::holds_alternative<array_t>(outputs.value()))
    {
        for (const auto& element: std::get<array_t>(outputs.value()))
        {
            if (!std::holds_alternative<object_t>(element.value()))
                return error::bitcoind::type_error;

            for (const auto& pair: std::get<object_t>(element.value()))
                if (const auto fault = append(pair.first, pair.second))
                    return fault;
        }
    }
    else
    {
        return error::bitcoind::type_error;
    }

    out = { tx_version, ins, outs, lock_time };
    return error::bitcoind::success;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
