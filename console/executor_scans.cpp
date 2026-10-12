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
#include "executor.hpp"
#include "localize.hpp"

#include <algorithm>
#include <cmath>

namespace libbitcoin {
namespace server {

using namespace network;
using namespace system;

constexpr double to_double(auto integer)
{
    return 1.0 * integer;
}

// fork flag transitions (candidate chain).
void executor::scan_flags() const
{
    const auto start = logger::now();
    constexpr auto flag_bits = to_bits(sizeof(chain::flags));
    const auto error = code{ database::error::integrity }.message();
    const auto top = query_.get_top_candidate();
    uint32_t flags{};

    logger(BS_OPERATION_INTERRUPT);

    for (size_t height{}; !canceled() && height <= top; ++height)
    {
        database::context ctx{};
        const auto link = query_.to_candidate(height);
        if (!query_.get_context(ctx, link) || (ctx.height != height))
        {
            logger(std::format("Error: {}", error));
            return;
        }

        if (ctx.flags != flags)
        {
            const binary prev{ flag_bits, to_big_endian(flags) };
            const binary next{ flag_bits, to_big_endian(ctx.flags) };
            logger(std::format("Forked from [{}] to [{}] at [{}:{}]",
                prev.encoded(), next.encoded(),
                encode_hash(query_.get_header_key(link)), height));
            flags = ctx.flags;
        }
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    const auto span = duration_cast<milliseconds>(logger::now() - start);
    logger(std::format("Scanned {} headers for rule forks in {} ms.", top,
        span.count()));
}

// hashmap bucket fill rates.
void executor::scan_buckets() const
{
    constexpr auto block_frequency = 10'000u;
    constexpr auto tx_frequency = 1'000'000u;
    constexpr auto put_frequency = 10'000'000u;

    logger(BS_OPERATION_INTERRUPT);

    auto filled = zero;
    auto bucket = max_size_t;
    auto start = logger::now();
    while (!canceled() && (++bucket < query_.header_buckets()))
    {
        const auto top = query_.top_header(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % block_frequency))
            logger(std::format("header" BS_READ_ROW, bucket,
                duration_cast<seconds>(logger::now() - start).count()));
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format("header" BS_READ_ROW, (to_double(filled) / bucket),
        span.count()));

    // ------------------------------------------------------------------------

    filled = zero;
    bucket = max_size_t;
    start = logger::now();
    while (!canceled() && (++bucket < query_.tx_buckets()))
    {
        const auto top = query_.top_tx(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % tx_frequency))
            logger(std::format("tx" BS_READ_ROW, bucket,
                duration_cast<seconds>(logger::now() - start).count()));
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(std::format("tx" BS_READ_ROW, (to_double(filled) / bucket),
        span.count()));

    // ------------------------------------------------------------------------

    filled = zero;
    bucket = max_size_t;
    start = logger::now();
    while (!canceled() && (++bucket < query_.ins_buckets()))
    {
        const auto top = query_.top_point(bucket);
        if (!top.is_terminal())
            ++filled;

        if (is_zero(bucket % put_frequency))
            logger(std::format("point" BS_READ_ROW, bucket,
                duration_cast<seconds>(logger::now() - start).count()));
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    span = duration_cast<seconds>(logger::now() - start);
    logger(std::format("point" BS_READ_ROW, (to_double(filled) / bucket),
        span.count()));
}

// hashmap head filter (derived as in hashhead).
template <typename Schema>
struct head_filter
{
    static constexpr size_t m = to_bits(Schema::cell) - Schema::link::bits;
    static constexpr size_t k_max = (m < two) ? zero :
        std::min(m, bits<uint64_t> / ceilinged_log2(m));
    using type = bloom<m, k_max>;
};

// hashmap bucket length distribution and filter collisions of one head.
template <typename Schema>
void executor::scan_collisions(const std::string& name, size_t records,
    size_t buckets, size_t k, size_t frequency,
    const auto& get_key) const
{
    using namespace database;
    using bucket_t = typename Schema::link::integer;
    using filter_t = typename head_filter<Schema>::type;
    using filter = typename filter_t::type;
    constexpr auto filtered = !filter_t::disabled;
    constexpr auto empty = unmask_right<filter>(head_filter<Schema>::m);
    constexpr auto points = is_same_type<typename Schema::key, chain::point>;

    if (is_zero(buckets))
        return;

    std_vector<uint8_t> lengths(buckets, zero);
    std_vector<filter> filters(filtered ? buckets : zero, empty);
    const auto count = possible_narrow_cast<bucket_t>(buckets);
    const auto start = logger::now();
    size_t collisions{};
    size_t nulls{};
    size_t index{};

    for (; !canceled() && index < records; ++index)
    {
        if (is_zero(index % frequency))
            logger(std::format("{}" BS_READ_ROW, name, index,
                duration_cast<seconds>(logger::now() - start).count()));

        const auto key = get_key(index);
        if constexpr (points)
        {
            if (key.index() == chain::point::null_index)
            {
                ++nulls;
                continue;
            }
        }

        const auto bucket = keys::bucket(key, count);
        auto& length = lengths.at(bucket);
        if (length < max_uint8)
            ++length;

        if constexpr (filtered)
        {
            auto& value = filters.at(bucket);
            const auto next = filter_t::screen(value, keys::thumb(key), k);
            collisions += to_int(filter_t::is_collision(value, next));
            value = next;
        }
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    filters.clear();
    filters.shrink_to_fit();

    std_array<size_t, add1<size_t>(max_uint8)> frequencies{};
    for (const auto length: lengths)
        ++frequencies.at(length);

    lengths.clear();
    lengths.shrink_to_fit();

    const auto puts = index - nulls;
    const auto load = to_double(puts) / buckets;
    const auto filled = buckets - frequencies.front();
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format("{}: {} keys in {}s buckets {} filled {} load {:.6f} "
        "fill {:.6f} (poisson {:.6f})", name, puts, span.count(), buckets,
        filled, load, (to_double(filled) / buckets), (1.0 - std::exp(-load))));

    if constexpr (points)
        logger(std::format("{}: {} null points excluded", name, nulls));

    if constexpr (filtered)
        logger(std::format("{}: k {} m {} collisions {} rate {:.7f}", name, k,
            head_filter<Schema>::m, collisions, (to_double(collisions) /
                std::max(puts, one))));

    auto poisson = std::exp(-load);
    for (size_t length{}; length < frequencies.size(); ++length)
    {
        if (is_nonzero(length))
            poisson *= load / to_double(length);

        if (is_nonzero(frequencies.at(length)))
            logger(std::format("{}: length {} buckets {} (poisson {:.0f})",
                name, length, frequencies.at(length), (poisson * buckets)));
    }
}

// hashmap collision distributions, one head at a time.
void executor::scan_collisions()
{
    using namespace database;
    constexpr auto block_frequency = 10'000u;
    constexpr auto tx_frequency = 1'000'000u;
    constexpr auto put_frequency = 10'000'000u;

    logger(BS_OPERATION_INTERRUPT);

    scan_collisions<schema::header>("header", query_.header_records(),
        query_.header_buckets(), store_.header.filter_k(), block_frequency,
        [&](size_t index)
        {
            return query_.get_header_key(possible_narrow_cast<
                header_link::integer>(index));
        });

    scan_collisions<schema::transaction>("tx", query_.tx_records(),
        query_.tx_buckets(), store_.tx.filter_k(), tx_frequency,
        [&](size_t index)
        {
            return query_.get_tx_key(possible_narrow_cast<
                tx_link::integer>(index));
        });

    scan_collisions<schema::strong_tx>("strong_tx",
        query_.strong_tx_records(), query_.strong_tx_buckets(),
        store_.strong_tx.filter_k(), tx_frequency,
        [&](size_t index)
        {
            return store_.strong_tx.get_key(possible_narrow_cast<
                table::strong_tx::link::integer>(index));
        });

    scan_collisions<schema::ins>("ins", query_.ins_records(),
        query_.ins_buckets(), store_.ins.filter_k(), put_frequency,
        [&](size_t index)
        {
            return query_.get_point_key(possible_narrow_cast<
                ins_link::integer>(index));
        });
}

} // namespace server
} // namespace libbitcoin
