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
#include <string>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/bitcoind_json.hpp>

namespace libbitcoin {
namespace server {
namespace btcd {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

boost::json::object search_transaction(const node::query& query,
    const database::tx_link& link, const chain::transaction& tx,
    const std::set<std::string>& filter, bool prevouts, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    auto model = boost::json::value_from(bitcoind(tx)).as_object();
    inject_tx_context(model, query, link);

    // Prevout resolution serves the prevOut extension and the filter.
    const auto populated = (prevouts || !filter.empty()) &&
        !tx.is_coinbase() && query.populate_with_metadata(tx);

    if (populated)
    {
        size_t index{};
        boost::json::array kept{};
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
                boost::json::array addresses{};
                if (!ad.empty())
                    addresses.emplace_back(std::move(ad));

                const auto value = in->prevout->value() / 
                    to_floating(chain::satoshi_per_bitcoin);

                item.as_object()["prevOut"] = boost::json::object
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
        boost::json::array kept{};
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

BC_POP_WARNING()

} // namespace btcd
} // namespace server
} // namespace libbitcoin
