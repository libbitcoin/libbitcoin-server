/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/estimator.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <ranges>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
constexpr auto relaxed = std::memory_order_relaxed;

// public
// ----------------------------------------------------------------------------

estimator::ptr estimator::create() NOEXCEPT
{
    return { new estimator{} ,[](estimator* ptr) NOEXCEPT { delete ptr; } };
}

uint64_t estimator::estimate(size_t target, mode mode) const NOEXCEPT
{
    // max_uint64 is failure sentinel (and unachievable/invalid as a fee).
    auto estimate = max_uint64;
    constexpr size_t large = horizon::large;
    if (target >= large)
        return estimate;

    // Valid results are effectively limited to at least 1 sat/vb.
    // threshold_fee is thread safe but values are affected during update. 
    switch (mode)
    {
        case mode::basic:
        {
            estimate = compute(target, confidence::high);
            break;
        }
        case mode::geometric:
        {
            estimate = compute(target, confidence::high, true);
            break;
        }
        case mode::economical:
        {
            const auto target1 = to_half(target);
            const auto target2 = std::min(one, target);
            const auto target3 = std::min(large, two * target);
            const auto fee1 = compute(target1, confidence::low);
            const auto fee2 = compute(target2, confidence::mid);
            const auto fee3 = compute(target3, confidence::high);
            estimate = std::max({ fee1, fee2, fee3 });
            break;
        }
        case mode::conservative:
        {
            const auto target1 = to_half(target);
            const auto target2 = std::min(one, target);
            const auto target3 = std::min(large, two * target);
            const auto fee1 = compute(target1, confidence::low);
            const auto fee2 = compute(target2, confidence::mid);
            const auto fee3 = compute(target3, confidence::high);
            estimate = std::max({ fee1, fee2, fee3 });
            break;
        }
    }

    return estimate;
}

bool estimator::initialize(std::atomic_bool& cancel, const node::query& query,
    size_t count) NOEXCEPT
{
    if (is_zero(count))
        return true;

    const auto top = query.get_top_confirmed();
    if (sub1(count) > top)
        return false;

    rate_sets blocks{};
    const auto start = top - sub1(count);
    return query.get_branch_fees(cancel, blocks, start, count) &&
        initialize(blocks);
}

bool estimator::push(const node::query& query) NOEXCEPT
{
    if (is_add_overflow(top_height(), one))
        return false;

    rates block{};
    const auto link = query.to_confirmed(add1(top_height()));
    return query.get_block_fees(block, link) && push(block);
}

bool estimator::pop(const node::query& query) NOEXCEPT
{
    if (is_subtract_overflow(top_height(), one))
        return false;

    rates block{};
    const auto link = query.to_confirmed(sub1(top_height()));
    return query.get_block_fees(block, link) && pop(block);
}

size_t estimator::top_height() const NOEXCEPT
{
    return fees_.top_height;
}

// protected
// ----------------------------------------------------------------------------

estimator::accumulator& estimator::history() NOEXCEPT
{
    return fees_;
}

const estimator::accumulator& estimator::history() const NOEXCEPT
{
    return fees_;
}

bool estimator::initialize(const rate_sets& blocks) NOEXCEPT
{
    const auto count = blocks.size();
    if (is_zero(count))
        return true;

    if (system::is_add_overflow(fees_.top_height, sub1(count)))
        return false;

    auto height = fees_.top_height;
    fees_.top_height += sub1(count);

    // TODO: could be parallel by block.
    for (const auto& block: blocks)
        if (!update(block, height++, true))
            return false;

    return true;
}

// Blocks must be pushed in order (but independent of chain index).
bool estimator::push(const rates& block) NOEXCEPT
{
    decay(true);
    return update(block, ++fees_.top_height, true);
}

// Blocks must be pushed in order (but independent of chain index).
bool estimator::pop(const rates& block) NOEXCEPT
{
    const auto result = update(block, fees_.top_height, false);
    decay(false);
    --fees_.top_height;
    return result;
}

uint64_t estimator::compute(size_t target, double confidence,
    bool geometric) const NOEXCEPT
{
    const auto threshold = [](double part, double total, size_t) NOEXCEPT
    {
        return part / total;
    };

    // Geometric distribution approximation, not a full Markov process.
    const auto markov = [](double part, double total, size_t target) NOEXCEPT
    {
        return system::power(part / total, target);
    };

    const auto call = [&](const auto& buckets) NOEXCEPT
    {
        BC_PUSH_WARNING(NO_UNGUARDED_POINTERS)
        const auto& contribution = geometric ? markov : threshold;
        BC_POP_WARNING()

        constexpr auto magic_number = 2u;
        const auto at_least_four = magic_number * add1(target);
        double total{}, part{};
        auto index = buckets.size();
        auto found = index;
        for (const auto& bucket: std::views::reverse(buckets))
        {
            --index;
            total += to_floating(bucket.total.load(relaxed));
            part += to_floating(bucket.confirmed.at(target).load(relaxed));
            if (total < at_least_four)
                continue;

            if (contribution(part, total, target) > (1.0 - confidence))
                break;

            found = index;
        }

        if (found == buckets.size())
            return max_uint64;

        const auto minimum = sizing::min * std::pow(sizing::step, found);
        return to_ceilinged_integer<uint64_t>(minimum);
    };

    if (target < horizon::small)  return call(fees_.small);
    if (target < horizon::medium) return call(fees_.medium);
    if (target < horizon::large)  return call(fees_.large);
    return max_uint64;
}

// private
// ----------------------------------------------------------------------------

void estimator::decay(bool push) NOEXCEPT
{
    // Not thread safe (use sequentially by block).
    const auto factor = to_scale_factor(push);
    decay(fees_.large, factor);
    decay(fees_.medium, factor);
    decay(fees_.small, factor);
}

void estimator::decay(auto& buckets, double factor) NOEXCEPT
{
    // Not thread safe (apply sequentially by block).
    const auto call = [factor](auto& count) NOEXCEPT
    {
        const auto value = count.load(relaxed);
        count.store(system::to_floored_integer(value * factor), relaxed);
    };

    for (auto& bucket: buckets)
    {
        call(bucket.total);
        for (auto& count: bucket.confirmed)
            call(count);
    }
}

bool estimator::update(const rates& block, size_t height, bool push) NOEXCEPT
{
    // std::log (replace static with constexpr in c++26).
    static const auto growth = std::log(sizing::step);
    std::array<size_t, sizing::count> counts{};

    for (const auto& tx: block)
    {
        if (is_zero(tx.bytes))
            return false;

        if (is_zero(tx.fee))
            continue;

        const auto rate = to_floating(tx.fee) / tx.bytes;
        if (rate < sizing::min)
            continue;

        // Clamp overflow to last bin.
        const auto bin = std::log(rate / sizing::min) / growth;
        ++counts.at(std::min(to_floored_integer(bin), sub1(sizing::count)));
    }

    // At age zero scale term is one.
    const auto age = top_height() - height;
    const auto scale = to_scale_term(age);
    const auto call = [&](auto& buckets) NOEXCEPT
    {
        // The array count of the buckets element type.
        const auto horizon = buckets.front().confirmed.size();

        size_t bin{};
        for (const auto count: counts)
        {
            if (is_zero(count))
            {
                ++bin;
                continue;
            }

            auto& bucket = buckets.at(bin++);
            const auto scaled = to_floored_integer(count * scale);
            const auto signed_term = push ? scaled : twos_complement(scaled);

            bucket.total.fetch_add(signed_term, relaxed);
            for (auto target = age; target < horizon; ++target)
                bucket.confirmed.at(target).fetch_add(signed_term, relaxed);
        }
    };

    call(fees_.large);
    call(fees_.medium);
    call(fees_.small);
    return true;
}

} // namespace server
} // namespace libbitcoin
