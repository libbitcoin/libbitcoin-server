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
#include <bitcoin/server/serializers/bitcoind_block_stats.hpp>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

// The truncated median (the mean of the middle pair for even counts).
static uint64_t truncated_median(std::vector<uint64_t>& values) NOEXCEPT
{
    if (values.empty())
        return {};

    std::ranges::sort(values);
    const auto middle = to_half(values.size());
    return is_even(values.size()) ?
        to_half(values.at(sub1(middle)) + values.at(middle)) :
        values.at(middle);
}

// The 10th/25th/50th/75th/90th fee rate percentiles, weighted by tx weight.
static std::array<uint64_t, 5> fee_rate_percentiles(
    std::vector<std::pair<uint64_t, uint64_t>>& rates,
    uint64_t total_weight) NOEXCEPT
{
    std::array<uint64_t, 5> out{};
    if (rates.empty())
        return out;

    std::ranges::sort(rates);
    const std::array<double, 5> cuts
    {
        total_weight / 10.0, total_weight / 4.0, total_weight / 2.0,
        (3.0 * total_weight) / 4.0, (9.0 * total_weight) / 10.0
    };

    size_t next{};
    uint64_t cumulative{};
    for (const auto& [rate, weight]: rates)
    {
        cumulative += weight;
        while (next < cuts.size() && cumulative >= cuts.at(next))
            out.at(next++) = rate;
    }

    while (next < cuts.size())
        out.at(next++) = rates.back().first;

    return out;
}

object_t block_stats(const chain::block& block, size_t height,
    uint32_t median_time, uint64_t subsidy) NOEXCEPT
{
    const auto& txs = *block.transactions_ptr();

    uint64_t total_output{}, total_fee{}, max_fee{}, max_fee_rate{};
    uint64_t min_fee = max_uint64, min_fee_rate = max_uint64;
    uint64_t inputs{}, outputs{}, utxos{};
    uint64_t max_tx_size{}, min_tx_size = max_uint64;
    uint64_t total_size{}, total_weight{};
    uint64_t witness_txs{}, witness_size{}, witness_weight{};
    std::vector<uint64_t> fees{}, sizes{};
    std::vector<std::pair<uint64_t, uint64_t>> rates{};

    for (const auto& tx: txs)
    {
        uint64_t tx_total_output{};
        for (const auto& out: *tx->outputs_ptr())
        {
            tx_total_output += out->value();

            // The genesis coinbase does not add to the utxo set, and
            // unspendable outputs are not included in it.
            if (is_zero(height) || out->script().is_unspendable())
                continue;

            ++utxos;
        }

        outputs += tx->outputs_ptr()->size();
        if (tx->is_coinbase())
            continue;

        inputs += tx->inputs_ptr()->size();
        total_output += tx_total_output;

        const uint64_t tx_size = tx->serialized_size(true);
        max_tx_size = std::max(max_tx_size, tx_size);
        min_tx_size = std::min(min_tx_size, tx_size);
        sizes.push_back(tx_size);
        total_size += tx_size;

        const auto weight = tx->weight();
        total_weight += weight;

        if (tx->is_segregated())
        {
            ++witness_txs;
            witness_size += tx_size;
            witness_weight += weight;
        }

        uint64_t tx_total_input{};
        for (const auto& in: *tx->inputs_ptr())
            tx_total_input += in->prevout->value();

        const auto fee = floored_subtract(tx_total_input, tx_total_output);
        fees.push_back(fee);
        max_fee = std::max(max_fee, fee);
        min_fee = std::min(min_fee, fee);
        total_fee += fee;

        // The fee rate is satoshis per virtual byte.
        const auto rate = is_zero(weight) ? zero :
            (chain::light_weight_factor * fee) / weight;
        rates.emplace_back(rate, weight);
        max_fee_rate = std::max(max_fee_rate, rate);
        min_fee_rate = std::min(min_fee_rate, rate);
    }

    const auto percentiles = fee_rate_percentiles(rates, total_weight);
    array_t rates_result(percentiles.size());
    std::ranges::copy(percentiles, rates_result.begin());

    // The count of non-coinbase txs (zero for a coinbase-only block).
    const auto paying = sub1(txs.size());

    return object_t
    {
        { "avgfee", is_zero(paying) ? zero : total_fee / paying },
        { "avgfeerate", is_zero(total_weight) ? zero :
            (chain::light_weight_factor * total_fee) / total_weight },
        { "avgtxsize", is_zero(paying) ? zero : total_size / paying },
        { "blockhash", encode_hash(block.hash()) },
        { "feerate_percentiles", rates_result },
        { "height", height },
        { "ins", inputs },
        { "maxfee", max_fee },
        { "maxfeerate", max_fee_rate },
        { "maxtxsize", max_tx_size },
        { "medianfee", truncated_median(fees) },
        { "mediantime", median_time },
        { "mediantxsize", truncated_median(sizes) },
        { "minfee", min_fee == max_uint64 ? zero : min_fee },
        { "minfeerate", min_fee_rate == max_uint64 ? zero : min_fee_rate },
        { "mintxsize", min_tx_size == max_uint64 ? zero : min_tx_size },
        { "outs", outputs },
        { "subsidy", subsidy },
        { "swtotal_size", witness_size },
        { "swtotal_weight", witness_weight },
        { "swtxs", witness_txs },
        { "time", block.header().timestamp() },
        { "total_out", total_output },
        { "total_size", total_size },
        { "total_weight", total_weight },
        { "totalfee", total_fee },
        { "txs", txs.size() },
        { "utxo_increase", delta(outputs, inputs) },
        { "utxo_increase_actual", delta(utxos, inputs) }
    };
}

} // namespace server
} // namespace libbitcoin
