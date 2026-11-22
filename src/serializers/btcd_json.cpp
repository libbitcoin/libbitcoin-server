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
#include <bitcoin/server/serializers/btcd_json.hpp>

#include <set>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/bitcoind_json.hpp>
#include <bitcoin/server/utilities/address.hpp>

namespace libbitcoin {
namespace server {
namespace btcd {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

static size_t required_signatures(const chain::script& script) NOEXCEPT
{
    using namespace chain;
    switch (script.output_pattern())
    {
        case script_pattern::pay_multisig:
            return operation::opcode_to_positive(script.ops().front().code());
        case script_pattern::pay_public_key:
        case script_pattern::pay_key_hash:
        case script_pattern::pay_script_hash:
        case script_pattern::pay_witness_key_hash:
        case script_pattern::pay_witness_script_hash:
        case script_pattern::pay_witness_v1_taproot:
            return one;
        default:
            return zero;
    }
}

// btcd scripts carry reqSigs and addresses, no descriptor, and signatures
// without sighash types.
static void inject_scripts(system::json::object& out,
    const chain::transaction& tx, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness, uint32_t flags) NOEXCEPT
{
    auto output = out.at("vout").as_array().begin();
    for (const auto& put: *tx.outputs_ptr())
    {
        auto& script = (output++)->as_object().at("scriptPubKey").as_object();
        const auto signatures = required_signatures(put->script());
        if (!is_zero(signatures))
            script["reqSigs"] = signatures;

        auto address = to_address(put->script(), p2kh, p2sh, witness);
        if (!address.empty())
        {
            system::json::array addresses{};
            addresses.emplace_back(address);
            script["address"] = std::move(address);
            script["addresses"] = std::move(addresses);
        }
    }

    if (tx.is_coinbase())
        return;

    auto input = out.at("vin").as_array().begin();
    for (const auto& in: *tx.inputs_ptr())
    {
        auto& script = (input++)->as_object().at("scriptSig").as_object();
        script["asm"] = in->script().to_string(flags, true);
    }
}

system::json::object verbose_transaction(const node::query& query,
    const database::tx_link& link, const chain::transaction& tx,
    const std::set<std::string>& filter, bool prevouts, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    auto model = system::json::value_from(bitcoind(tx, flags)).as_object();
    inject_tx_context(model, query, link);
    inject_scripts(model, tx, p2kh, p2sh, witness, flags);
    model.stable_erase("in_active_chain");

    size_t height{};
    if (!query.get_tx_height(height, link))
        model.stable_erase("confirmations");

    // Prevout resolution serves the prevOut extension and the filter.
    const auto populated = (prevouts || !filter.empty()) &&
        !tx.is_coinbase() && query.populate_with_metadata(tx);

    if (populated)
    {
        size_t index{};
        system::json::array kept{};
        auto& inputs = model.at("vin").as_array();
        for (const auto& in: *tx.inputs_ptr())
        {
            auto ad = to_address(in->prevout->script(), p2kh, p2sh, witness);
            if (!filter.empty() && !filter.contains(ad))
            {
                ++index;
                continue;
            }

            auto item = std::move(inputs.at(index++));
            if (prevouts)
            {
                system::json::array addresses{};
                if (!ad.empty())
                    addresses.emplace_back(std::move(ad));

                const auto value = in->prevout->value() / 
                    to_floating(chain::satoshi_per_bitcoin);

                item.as_object()["prevOut"] = system::json::object
                {
                    { "addresses", std::move(addresses) },
                    { "value", value }
                };
            }

            kept.emplace_back(std::move(item));
        }

        inputs = std::move(kept);
    }

    if (!filter.empty())
    {
        size_t index{};
        system::json::array kept{};
        auto& outputs = model.at("vout").as_array();
        for (const auto& out: *tx.outputs_ptr())
        {
            const auto ad = to_address(out->script(), p2kh, p2sh, witness);
            if (filter.contains(ad))
                kept.emplace_back(std::move(outputs.at(index)));

            ++index;
        }

        outputs = std::move(kept);
    }

    return model;
}

system::json::object search_transaction(const node::query& query,
    const database::tx_link& link, const chain::transaction& tx,
    const std::set<std::string>& filter, bool prevouts, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    auto model = verbose_transaction(query, link, tx, filter, prevouts, p2kh,
        p2sh, witness, flags);

    model["hash"] = std::string{};
    model["size"] = std::string{};
    model["vsize"] = std::string{};
    model["weight"] = std::string{};
    return model;
}

BC_POP_WARNING()

} // namespace btcd
} // namespace server
} // namespace libbitcoin
