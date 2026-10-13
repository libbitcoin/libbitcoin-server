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

#include <map>
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

fee_histogram to_compact_fee_histogram(const database::fee_rates& rates,
    size_t bin) NOEXCEPT
{
    constexpr auto growth = 1.1;
    constexpr uint64_t tenths = 10;

    std::map<uint64_t, size_t, std::greater<>> sizes{};
    for (const auto& tx: rates)
    {
        const auto fee = ceilinged_multiply(tx.fee, tenths);
        const auto rate = is_zero(tx.bytes) ? 0_u64 : fee / tx.bytes;
        auto& size = sizes[rate];
        size = ceilinged_add(size, tx.bytes);
    }

    const auto to_rate = [](uint64_t rate) NOEXCEPT
    {
        return to_floating(rate) / tenths;
    };

    size_t size{};
    uint64_t last{};
    fee_histogram out{};
    auto limit = to_floating(bin);
    for (const auto& [rate, bytes]: sizes)
    {
        if (is_nonzero(size) && to_floating(bytes) > 2.0 * limit)
        {
            out.emplace_back(to_rate(last), size);
            limit *= growth;
            size = zero;
        }

        size = ceilinged_add(size, bytes);
        if (to_floating(size) > limit)
        {
            out.emplace_back(to_rate(rate), size);
            limit *= growth;
            size = zero;
        }

        last = rate;
    }

    return out;
}

// mempool.dat
// ----------------------------------------------------------------------------

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

constexpr uint64_t mempool_unobfuscated = 1;
constexpr uint64_t mempool_obfuscated = 2;
constexpr size_t mempool_key_size = 8;
constexpr size_t mempool_key_offset = sizeof(uint64_t) + one;
constexpr size_t mempool_body_offset = mempool_key_offset + mempool_key_size;

bool serialize_mempool(data_chunk& out, const node::query& query) NOEXCEPT
{
    database::tx_links links{};
    if (query.get_confirmable_txs(links))
        return false;

    out.clear();
    write::bytes::data sink{ out };
    sink.write_8_bytes_little_endian(mempool_unobfuscated);
    sink.write_8_bytes_little_endian(links.size());
    for (const auto& link: links)
    {
        uint32_t time{};
        const auto tx = query.get_transaction(link, true);
        if (!tx || !query.get_pooled_time(time, link))
            return false;

        tx->to_data(sink, true);
        sink.write_8_bytes_little_endian(time);
        sink.write_8_bytes_little_endian(zero);
    }

    sink.write_variable(zero);
    sink.write_variable(zero);
    sink.flush();
    return true;
}

// Version 2 obfuscates each byte that follows its key, by file position.
bool parse_mempool(chain::transaction_cptrs& out, data_chunk& data) NOEXCEPT
{
    read::bytes::copy source{ data };
    const auto version = source.read_8_bytes_little_endian();
    if (version == mempool_obfuscated)
    {
        if (source.read_size() != mempool_key_size ||
            data.size() < mempool_body_offset)
            return false;

        source.skip_bytes(mempool_key_size);
        for (auto index = mempool_body_offset; index < data.size(); ++index)
            data.at(index) ^= data.at(mempool_key_offset +
                (index % mempool_key_size));
    }
    else if (version != mempool_unobfuscated)
    {
        return false;
    }

    const auto count = source.read_8_bytes_little_endian();
    for (uint64_t tx{}; tx < count && source; ++tx)
    {
        out.push_back(to_shared<chain::transaction>(source, true));
        source.skip_bytes(sizeof(uint64_t) + sizeof(int64_t));
    }

    return !!source;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
