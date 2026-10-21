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
#include <bitcoin/server/utilities/chain.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

uint32_t median_time(const node::query& query,
    const system::settings& settings,
    const database::header_link& link) NOEXCEPT
{
    database::context ctx{};
    if (query.get_context(ctx, query.to_confirmed_child(link)))
        return ctx.mtp;

    // The top block has no child, its promoted chain state carries the value.
    const auto key = query.get_header_key(link);
    const auto state = query.get_confirmed_chain_state(settings, key);
    if (!state)
        return {};

    return chain::chain_state{ *state, settings }.context().median_time_past;
}

BC_POP_WARNING()

fee_histogram to_fee_histogram(database::fee_rates rates, size_t bin) NOEXCEPT
{
    const auto rate = [](const database::fee_rate& tx) NOEXCEPT
    {
        return is_zero(tx.bytes) ? 0.0 : to_floating(tx.fee) / tx.bytes;
    };

    std::ranges::sort(rates, [&](const auto& left, const auto& right) NOEXCEPT
    {
        return rate(left) > rate(right);
    });

    double last{};
    size_t size{};
    fee_histogram out{};
    for (const auto& tx: rates)
    {
        const auto current = rate(tx);
        if (size > bin && current != last)
        {
            out.emplace_back(last, size);
            size = zero;
        }

        last = current;
        size = ceilinged_add(size, tx.bytes);
    }

    if (is_nonzero(size))
        out.emplace_back(last, size);

    return out;
}

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

bool get_pool_snapshot(boost::json::value& out,
    const node::query& query) NOEXCEPT
{
    database::fee_rates rates{};
    if (!query.get_confirmable_fees(rates))
        return false;

    size_t vsize{};
    uint64_t fee{};
    for (const auto& rate: rates)
    {
        vsize = ceilinged_add(vsize, rate.bytes);
        fee = ceilinged_add(fee, rate.fee);
    }

    // Preserve Esplora's 50,000-vbyte histogram bins and units.
    const auto bins = to_fee_histogram(rates, 50'000);
    out = boost::json::object
    {
        { "count", rates.size() },
        { "vsize", vsize },
        { "total_fee", fee },
        { "fee_histogram", boost::json::value_from(bins) }
    };
    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
