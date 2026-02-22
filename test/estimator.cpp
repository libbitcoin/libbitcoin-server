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
#include "test.hpp"

BOOST_AUTO_TEST_SUITE(estimator_tests)

using namespace system;

struct acessor
  : server::estimator
{
    typedef std::shared_ptr<acessor> ptr;

    static acessor::ptr create() NOEXCEPT
    {
        return { new acessor(),[](acessor* ptr) NOEXCEPT { delete ptr; } };
    }

    using rates = estimator::rates;
    using rate_sets = estimator::rate_sets;
    using confidence = estimator::confidence;
    using horizon = estimator::horizon;
    using sizing = estimator::sizing;
    using estimator::decay_rate;
    using estimator::to_scale_term;
    using estimator::to_scale_factor;
    using estimator::history;
    using estimator::initialize;
    using estimator::push;
    using estimator::pop;
    using estimator::compute;
};

// decay_rate

BOOST_AUTO_TEST_CASE(estimator__decay_rate__invoke__expected)
{
    const auto expected = std::pow(0.5, 1.0 / acessor::sizing::count);
    BOOST_REQUIRE_CLOSE(acessor::decay_rate(), expected, 0.000001);
}

// to_scale_term

BOOST_AUTO_TEST_CASE(estimator__to_scale_term__zero__one)
{
    BOOST_REQUIRE_EQUAL(acessor::to_scale_term(0u), 1.0);
}

BOOST_AUTO_TEST_CASE(estimator__to_scale_term__non_zero__expected)
{
    const auto rate = acessor::decay_rate();
    constexpr auto age = 42u;
    const auto expected = std::pow(rate, age);
    BOOST_REQUIRE_CLOSE(acessor::to_scale_term(age), expected, 0.000001);
}

// to_scale_factor

BOOST_AUTO_TEST_CASE(estimator__to_scale_factor__push_true__decay_rate)
{
    const auto rate = acessor::decay_rate();
    const auto expected = std::pow(rate, +1.0);
    BOOST_REQUIRE_CLOSE(acessor::to_scale_factor(true), expected, 0.000001);
}

BOOST_AUTO_TEST_CASE(estimator__to_scale_factor__push_false__inverse_decay_rate)
{
    const auto rate = acessor::decay_rate();
    const auto expected = std::pow(rate, -1.0);
    BOOST_REQUIRE_CLOSE(acessor::to_scale_factor(false), expected, 0.000001);
}

// top_height

BOOST_AUTO_TEST_CASE(estimator__top_height__default__zero)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
}

BOOST_AUTO_TEST_CASE(estimator__top_height__non_default__expected)
{
    const auto instance = acessor::create();
    instance->history().top_height = 42u;
    BOOST_REQUIRE_EQUAL(instance->top_height(), 42u);
}

// initialize

BOOST_AUTO_TEST_CASE(estimator__initialize__empty__true_height_unchanged)
{
    const auto instance = acessor::create();
    const acessor::rate_sets empty{};
    BOOST_REQUIRE(instance->initialize(empty));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, 0u);


}BOOST_AUTO_TEST_CASE(estimator__initialize__overflow__false_height_unchanged)
{
    const auto instance = acessor::create();
    instance->history().top_height = sub1(max_size_t);
    acessor::rate_sets blocks(3);
    BOOST_REQUIRE(!instance->initialize(blocks));
    BOOST_REQUIRE_EQUAL(instance->top_height(), sub1(max_size_t));
}

BOOST_AUTO_TEST_CASE(estimator__initialize__two_blocks__true_height_updated)
{
    const auto instance = acessor::create();
    acessor::rate_sets blocks(2);
    BOOST_REQUIRE(instance->initialize(blocks));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__single_block__populates_expected)
{
    const auto instance = acessor::create();

    // rate of 1/10 (0.1) in bin 0.
    const acessor::rates block{ { 10u, 1u } };
    const acessor::rate_sets blocks{ block };
    BOOST_REQUIRE(instance->initialize(blocks));

    constexpr size_t age{};
    const auto scale = acessor::to_scale_term(age);
    const auto scaled = to_floored_integer<size_t>(1u * scale);
    const auto& small0 = instance->history().small.at(0);

    BOOST_REQUIRE_EQUAL(small0.total, scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[2], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[3], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[4], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[5], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[6], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[7], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[8], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[9], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[10], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], scaled);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[1], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[2], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[45], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[46], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], scaled);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[2], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1005], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1006], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], scaled);
}

BOOST_AUTO_TEST_CASE(estimator__initialize__two_blocks_with_data__expected)
{
    // 1 tx, rate=0.1, bin=0
    // 2 tx, rate=0.1, bin=0
    // Expected total: floor(1 * decay_rate) + floor(2 * 1.0) = 0 + 2 = 2.
    const auto instance = acessor::create();
    const acessor::rates oldest{ { 10u, 1u } };
    const acessor::rates newest{ { 10u, 1u }, { 10u, 1u } };
    acessor::rate_sets blocks{ oldest, newest };
    BOOST_REQUIRE(instance->initialize(blocks));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().small.at(0).total, 2u);
}

// push

BOOST_AUTO_TEST_CASE(estimator__push__empty_block__decays_and_increments)
{
    const auto instance = acessor::create();
    constexpr auto initial = 100u;
    instance->history().small[0].total = initial;
    const auto factor = acessor::to_scale_factor(true);
    const auto expected = to_floored_integer<size_t>(initial * factor);
    const acessor::rates empty{};
    BOOST_REQUIRE(instance->push(empty));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, expected);
}

BOOST_AUTO_TEST_CASE(estimator__push__single_tx__populates_expected)
{
    const auto instance = acessor::create();

    // rate of 1/10 (0.1) in bin 0.
    const acessor::rates block{ { 10u, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 1u);

    constexpr size_t age{};
    const auto scale = acessor::to_scale_term(age);
    const auto scaled = to_floored_integer<size_t>(1u * scale);
    const auto& small0 = instance->history().small.at(0);

    BOOST_REQUIRE_EQUAL(small0.total, scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[2], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[3], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[4], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[5], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[6], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[7], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[8], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[9], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[10], scaled);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], scaled);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[1], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[2], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[45], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[46], scaled);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], scaled);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[2], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1005], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1006], scaled);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], scaled);
}

// pop

BOOST_AUTO_TEST_CASE(estimator__pop__empty_block__decays_and_decrements)
{
    const auto instance = acessor::create();
    instance->history().top_height = 1u;
    constexpr auto initial = 100u;
    instance->history().small[0].total = initial;
    const auto factor = acessor::to_scale_factor(false);
    const auto expected = to_floored_integer<size_t>(initial * factor);
    const acessor::rates empty{};
    BOOST_REQUIRE(instance->pop(empty));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);
    BOOST_REQUIRE_EQUAL(instance->history().small[0].total, expected);
}

BOOST_AUTO_TEST_CASE(estimator__pop__reverses_push__restores_state)
{
    const auto instance = acessor::create();

    // rate of 1/10 (0.1) in bin 0.
    const acessor::rates block{ { 10u, 1u } };
    BOOST_REQUIRE(instance->push(block));
    BOOST_REQUIRE(instance->pop(block));
    BOOST_REQUIRE_EQUAL(instance->top_height(), 0u);

    const auto& small0 = instance->history().small.at(0);

    BOOST_REQUIRE_EQUAL(small0.total, 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[1], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[3], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[4], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[5], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[6], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[7], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[8], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[9], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[10], 0u);
    BOOST_REQUIRE_EQUAL(small0.confirmed[11], 0u);

    const auto& medium0 = instance->history().medium.at(0);
    BOOST_REQUIRE_EQUAL(medium0.total, 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[1], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[45], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[46], 0u);
    BOOST_REQUIRE_EQUAL(medium0.confirmed[47], 0u);

    const auto& large0 = instance->history().large.at(0);
    BOOST_REQUIRE_EQUAL(large0.total, 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[0], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[2], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1005], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1006], 0u);
    BOOST_REQUIRE_EQUAL(large0.confirmed[1007], 0u);
}

// compute

BOOST_AUTO_TEST_CASE(estimator__compute__default_state__max_uint64)
{
    const auto instance = acessor::create();
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
    BOOST_REQUIRE_EQUAL(instance->compute(1, acessor::confidence::mid, true), max_uint64);
    BOOST_REQUIRE_EQUAL(instance->compute(50, acessor::confidence::low), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__insufficient_total__max_uint64)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;

    // < at_least_four=2 for target=0.
    constexpr auto value = 1u;
    instance->history().small[bin].total = value;
    instance->history().small[bin].confirmed[0] = value;
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__low_failure_basic__expected_fee)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;

    // 0/10 = 0 <= 0.05.
    constexpr auto failure = 0u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[0] = failure;
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__high_failure_basic__max_uint64)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;

    // 1/10 = 0.1 > 0.05.
    constexpr auto failure = 1u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[0] = failure;
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), max_uint64);
}

BOOST_AUTO_TEST_CASE(estimator__compute__multi_bin_basic__expected_fee)
{
    const auto instance = acessor::create();
    constexpr auto low_bin = 0u;
    constexpr auto high_bin = 1u;
    constexpr auto total = 10u;

    // high failure in low bin.
    constexpr auto low_failure = 10u;

    // low failure in high bin.
    constexpr auto high_failure = 0u;
    instance->history().small[low_bin].total = total;
    instance->history().small[low_bin].confirmed[0] = low_failure;
    instance->history().small[high_bin].total = total;
    instance->history().small[high_bin].confirmed[0] = high_failure;

    // Cumulative at high_bin: 0/10 = 0 <= 0.05, then at low_bin: 10/20 = 0.5 > 0.05, found=1.
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, high_bin));
    BOOST_REQUIRE_EQUAL(instance->compute(0, acessor::confidence::high), fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__geometric_target_one__matches_basic)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;
    constexpr auto failure = 0u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[1] = failure;
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    const auto basic = instance->compute(1, acessor::confidence::high, false);
    const auto geometric = instance->compute(1, acessor::confidence::high, true);
    BOOST_REQUIRE_EQUAL(basic, fee);
    BOOST_REQUIRE_EQUAL(geometric, fee);
}

BOOST_AUTO_TEST_CASE(estimator__compute__geometric_high_target__expected)
{
    const auto instance = acessor::create();
    constexpr auto bin = 0u;
    constexpr auto total = 10u;

    // p=0.1, pow(0.1,2)=0.01 < 0.05, so found=0.
    constexpr auto failure = 1u;
    instance->history().small[bin].total = total;
    instance->history().small[bin].confirmed[2] = failure;
    const auto fee = to_ceilinged_integer<uint64_t>(acessor::sizing::min * std::pow(acessor::sizing::step, bin));
    BOOST_REQUIRE_EQUAL(instance->compute(2, acessor::confidence::high, true), fee);

    // Contrast with basic: 0.1 > 0.05, would be max_uint64.
    BOOST_REQUIRE_EQUAL(instance->compute(2, acessor::confidence::high, false), max_uint64);
}

BOOST_AUTO_TEST_SUITE_END()
