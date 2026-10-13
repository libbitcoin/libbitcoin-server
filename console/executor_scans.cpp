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
    size_t height{};

    logger(BS_OPERATION_INTERRUPT);

    for (; !canceled() && height <= top; ++height)
    {
        database::context ctx{};
        const auto link = query_.to_candidate(height);
        if (!query_.get_context(ctx, link) || (ctx.height != height))
        {
            logger(std::format(BS_FLAGS_ERROR, error));
            return;
        }

        if (ctx.flags != flags)
        {
            const binary prev{ flag_bits, to_big_endian(flags) };
            const binary next{ flag_bits, to_big_endian(ctx.flags) };
            logger(std::format(BS_FLAGS_FORKED, prev.encoded(),
                next.encoded(),
                encode_hash(query_.get_header_key(link)), height));
            flags = ctx.flags;
        }
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    const auto span = duration_cast<milliseconds>(logger::now() - start);
    logger(std::format(BS_FLAGS_SCANNED, height, span.count()));
}

// Link keys are narrower than hashes, other keys are hashed.
template <typename Key>
constexpr bool is_linked() NOEXCEPT
{
    if constexpr (is_std_array<Key>)
        return (array_count<Key>) < hash_size;
    else
        return false;
}

// Null points (coinbase) and null search keys (unwritten rows) are excluded.
template <typename Key>
constexpr bool is_nullable() NOEXCEPT
{
    return is_same_type<Key, chain::point> || database::keys::is_search<Key>;
}

template <typename Key>
constexpr bool is_null(const Key& key) NOEXCEPT
{
    if constexpr (is_same_type<Key, chain::point>)
        return key.index() == chain::point::null_index;
    else if constexpr (database::keys::is_search<Key>)
        return key.value == null_hash;
    else
        return false;
}

static std::string to_grouped(size_t value)
{
    auto text = std::to_string(value);
    for (auto at = text.size(); at > 3u; at -= 3u)
        text.insert(at - 3u, ",");

    return text;
}

// hashmap bucket fill of one head (head only).
template <typename Table>
void executor::scan_buckets(const std::string& name, const Table& table,
    size_t records) const
{
    using key_t = typename Table::key;
    using bucket_t = typename Table::link::integer;
    constexpr auto linked = is_linked<key_t>();

    const auto buckets = table.buckets();
    if (is_zero(buckets) || canceled())
        return;

    const auto start = logger::now();
    size_t filled{};
    size_t bucket{};
    for (; !canceled() && bucket < buckets; ++bucket)
        if (!table.top(possible_narrow_cast<bucket_t>(bucket)).is_terminal())
            ++filled;

    const auto load = to_double(records) / buckets;
    const auto expect = linked ? std::min(load, 1.0) : 1.0 - std::exp(-load);
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_BUCKETS_ROW, name, to_grouped(bucket),
        to_grouped(records), load, (to_double(filled) / std::max(bucket, one)),
        expect, span.count()));
}

// hashmap bucket fill rates, all heads.
void executor::scan_buckets() const
{
    logger(BS_OPERATION_INTERRUPT);
    logger(BS_BUCKETS_HEADER);

    scan_buckets("header", store_.header, query_.header_records());
    scan_buckets("tx", store_.tx, query_.tx_records());
    scan_buckets("ins", store_.ins, query_.ins_records());
    scan_buckets("outs", store_.outs, query_.outs_records());
    scan_buckets("strong", store_.strong, query_.strong_records());
    scan_buckets("duplicate", store_.duplicate, query_.duplicate_records());
    scan_buckets("pool", store_.pool, query_.pool_records());
    scan_buckets("wtxid", store_.wtxid, query_.wtxid_records());

    if (canceled())
        logger(BS_OPERATION_CANCELED);
}

// hashmap bucket distribution and filter false positives of one head.
template <typename Table>
void executor::scan_collisions(const std::string& name, size_t records,
    size_t buckets, size_t k, string_list& distributions,
    string_list& filters, const auto& get_key) const
{
    using namespace database;
    using key_t = typename Table::key;
    using bucket_t = typename Table::link::integer;
    using filter_t = typename Table::filter_t;
    using filter = typename filter_t::type;
    constexpr auto m = Table::filter_bits;
    constexpr auto rows = 20_size;
    constexpr auto filtered = !filter_t::disabled;
    constexpr auto empty = unmask_right<filter>(m);
    constexpr auto nullable = is_nullable<key_t>();
    constexpr auto linked = is_linked<key_t>();

    if (is_zero(buckets))
        return;

    const auto kind = linked ? BS_COLLISIONS_LINK : BS_COLLISIONS_HASH;
    if constexpr (filtered)
        logger(std::format(BS_COLLISIONS_FILTERED, name, kind, m, k,
            Table::filter_k_max));
    else
        logger(std::format(BS_COLLISIONS_UNFILTERED, name, kind));

    std_vector<uint8_t> lengths(buckets, zero);
    std_vector<filter> screens(filtered ? buckets : zero, empty);
    const auto count = possible_narrow_cast<bucket_t>(buckets);
    const auto frequency = std::max(ceilinged_divide(records, rows), one);
    const auto start = logger::now();
    size_t window_collisions{};
    size_t window_walk{};
    size_t window_start{};
    size_t collisions{};
    size_t walk{};
    size_t nulls{};
    size_t index{};
    auto fp_model = 0.0;
    auto walk_model = 0.0;

    const auto progress = [&]() NOEXCEPT
    {
        const auto keyed = index - nulls;
        const auto window = keyed - window_start;
        const auto load = to_double(keyed) / buckets;
        const auto secs = duration_cast<seconds>(logger::now() - start);
        if constexpr (filtered)
        {
            if (is_zero(window))
                return;

            const auto middle = to_double(window_start + keyed) / 2.0 /
                buckets;
            const auto rate = Table::false_positive(k, middle);
            const auto cost = Table::expected_walk(k, middle);
            fp_model += rate * window;
            walk_model += cost * window;
            logger(std::format(BS_COLLISIONS_FILTERED_ROW,
                to_grouped(keyed), load,
                (to_double(window_collisions) / window), rate,
                (to_double(window_walk) / window), cost, secs.count()));

            window_collisions = zero;
            window_walk = zero;
        }
        else
        {
            logger(std::format(BS_COLLISIONS_UNFILTERED_ROW,
                to_grouped(keyed), load, secs.count()));
        }

        window_start = keyed;
    };

    for (; !canceled() && index < records; ++index)
    {
        if (is_nonzero(index) && is_zero(index % frequency))
            progress();

        const auto key = get_key(index);
        if (is_null(key))
        {
            ++nulls;
            continue;
        }

        const auto bucket = keys::bucket(key, count);
        auto& length = lengths.at(bucket);
        const auto prior = length;
        if (length < max_uint8)
            ++length;

        if constexpr (filtered)
        {
            auto& value = screens.at(bucket);
            const auto next = filter_t::screen(value, keys::thumb(key), k);
            if (filter_t::is_collision(value, next))
            {
                ++window_collisions;
                ++collisions;
                window_walk += prior;
                walk += prior;
            }

            value = next;
        }
    }

    progress();
    if (canceled())
        logger(BS_OPERATION_CANCELED);

    screens.clear();
    screens.shrink_to_fit();

    std_array<size_t, add1<size_t>(max_uint8)> frequencies{};
    for (const auto length: lengths)
        ++frequencies.at(length);

    lengths.clear();
    lengths.shrink_to_fit();

    // Expected distribution is poisson if hash keyed, perfect if link keyed.
    const auto keyed = index - nulls;
    const auto load = to_double(keyed) / buckets;
    const auto whole = std::floor(load);
    const auto part = load - whole;
    const auto perfect = [&](size_t length) NOEXCEPT
    {
        const auto size = to_double(length);
        if (size == whole) return (1.0 - part) * buckets;
        if (size == whole + 1.0) return part * buckets;
        return 0.0;
    };

    logger(BS_COLLISIONS_LENGTHS);
    auto poisson = std::exp(-load);
    auto squares = 0.0;
    auto hits = 0.0;
    auto longest = zero;
    for (size_t length{}; length < frequencies.size(); ++length)
    {
        if (is_nonzero(length))
            poisson *= load / to_double(length);

        const auto occurrences = frequencies.at(length);
        if (is_zero(occurrences))
            continue;

        const auto size = to_double(length);
        squares += to_double(occurrences) * size * size;
        hits += to_double(occurrences) * size * (size + 1.0) / 2.0;
        longest = length;
        const auto expect = linked ? perfect(length) : poisson * buckets;
        logger(std::format(BS_COLLISIONS_LENGTHS_ROW, length,
            to_grouped(occurrences),
            to_grouped(to_integer<size_t>(std::round(expect)))));
    }

    if constexpr (nullable)
        logger(std::format(BS_COLLISIONS_NULLS, to_grouped(nulls)));

    const auto divisor = to_double(std::max(keyed, one));
    const auto empty_load = is_zero(keyed);
    const auto filled = to_double(buckets - frequencies.front()) / buckets;
    const auto dispersion = empty_load ? 0.0 :
        (squares / buckets - load * load) / load;
    const auto walked = hits / divisor;
    const auto fill_expect = linked ? std::min(load, 1.0) :
        1.0 - std::exp(-load);
    const auto dispersion_expect = !linked ? 1.0 : empty_load ? 0.0 :
        part * (1.0 - part) / load;
    const auto walk_expect = !linked ? 1.0 + load / 2.0 : empty_load ? 0.0 :
        ((1.0 - part) * whole * (whole + 1.0) / 2.0 +
            part * (whole + 1.0) * (whole + 2.0) / 2.0) / load;

    distributions.push_back(std::format(BS_COLLISIONS_DISTRIBUTION_ROW,
        name, to_grouped(buckets), to_grouped(keyed), load, filled,
        fill_expect, dispersion, dispersion_expect, walked, walk_expect,
        longest));

    if constexpr (filtered)
    {
        const auto optimal = Table::optimal_k(keyed, buckets);
        filters.push_back(std::format(BS_COLLISIONS_FILTER_ROW, name, m, k,
            optimal, (to_double(collisions) / divisor),
            (fp_model / divisor), (to_double(walk) / divisor),
            (walk_model / divisor), Table::false_positive(k, load),
            Table::false_positive(optimal, load)));
    }
}

// hashmap collision distributions, one head at a time.
void executor::scan_collisions()
{
    using namespace database;
    string_list distributions{};
    string_list filters{};

    logger(BS_OPERATION_INTERRUPT);

    scan_collisions<table::header>("header", query_.header_records(),
        query_.header_buckets(), store_.header.filter_k(), distributions,
        filters, [&](size_t index)
        {
            return query_.get_header_key(possible_narrow_cast<
                header_link::integer>(index));
        });

    scan_collisions<table::transaction>("tx", query_.tx_records(),
        query_.tx_buckets(), store_.tx.filter_k(), distributions, filters,
        [&](size_t index)
        {
            return query_.get_tx_key(possible_narrow_cast<
                tx_link::integer>(index));
        });

    scan_collisions<table::strong>("strong",
        query_.strong_records(), query_.strong_buckets(),
        store_.strong.filter_k(), distributions, filters,
        [&](size_t index)
        {
            return store_.strong.get_key(possible_narrow_cast<
                table::strong::link::integer>(index));
        });

    scan_collisions<table::ins>("ins", query_.ins_records(),
        query_.ins_buckets(), store_.ins.filter_k(), distributions, filters,
        [&](size_t index)
        {
            return query_.get_point_key(possible_narrow_cast<
                ins_link::integer>(index));
        });

    scan_collisions<table::outs>("outs", query_.outs_records(),
        query_.outs_buckets(), store_.outs.filter_k(), distributions, filters,
        [&](size_t index)
        {
            return table::outs::key{ query_.get_outs_key(possible_narrow_cast<
                outs_link::integer>(index)) };
        });

    scan_collisions<table::wtxid>("wtxid", query_.wtxid_records(),
        query_.wtxid_buckets(), store_.wtxid.filter_k(), distributions,
        filters, [&](size_t index)
        {
            return table::wtxid::key{ query_.get_wtxid_key(
                possible_narrow_cast<pool_link::integer>(index)) };
        });

    logger(BS_COLLISIONS_DISTRIBUTION);
    for (const auto& row: distributions)
        logger(row);

    logger(BS_COLLISIONS_FILTER);
    for (const auto& row: filters)
        logger(row);
}

} // namespace server
} // namespace libbitcoin
