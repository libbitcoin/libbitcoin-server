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
#include "../test.hpp"

BC_AUTO_TEST_SUITE(extranonces_tests)

using namespace system;
using prefixes = std::vector<server::extranonces::prefix>;

// Allocate prefixes of the size until one is not drawn.
static prefixes exhaust(server::extranonces& pool, size_t size) NOEXCEPT
{
    prefixes out{};
    server::extranonces::prefix prefix{};
    while ((prefix = pool.allocate(size)))
        out.push_back(prefix);

    return out;
}

// No prefix of the set begins with another of the set.
static bool disjoint(const prefixes& set) NOEXCEPT
{
    for (const auto& left: set)
        for (const auto& right: set)
            if (left != right && left->size() <= right->size() &&
                std::equal(left->begin(), left->end(), right->begin()))
                return false;

    return true;
}

BC_AUTO_TEST_CASE(extranonces__allocate__zero_size__empty)
{
    const auto pool = to_shared<server::extranonces>();
    BC_REQUIRE(!pool->allocate(0));
    BC_REQUIRE_EQUAL(pool->size(), 0u);
}

BC_AUTO_TEST_CASE(extranonces__allocate__size__expected_size_in_use)
{
    const auto pool = to_shared<server::extranonces>();
    const auto prefix = pool->allocate(4);
    BC_REQUIRE(prefix);
    BC_REQUIRE_EQUAL(prefix->size(), 4u);
    BC_REQUIRE_EQUAL(pool->size(), 1u);
}

BC_AUTO_TEST_CASE(extranonces__allocate__dropped__released)
{
    const auto pool = to_shared<server::extranonces>();
    auto prefix = pool->allocate(4);
    prefix.reset();
    BC_REQUIRE_EQUAL(pool->size(), 0u);
}

BC_AUTO_TEST_CASE(extranonces__allocate__one_byte__at_most_256_disjoint)
{
    const auto pool = to_shared<server::extranonces>();
    const auto set = exhaust(*pool, 1);
    BC_REQUIRE(!set.empty());
    BC_REQUIRE(set.size() <= 256u);
    BC_REQUIRE(disjoint(set));
}

BC_AUTO_TEST_CASE(extranonces__allocate__mixed_sizes__disjoint)
{
    const auto pool = to_shared<server::extranonces>();
    auto set = exhaust(*pool, 1);
    const auto longer = exhaust(*pool, 2);
    set.insert(set.end(), longer.begin(), longer.end());
    BC_REQUIRE(disjoint(set));
}

BC_AUTO_TEST_CASE(extranonces__allocate__exhausted_then_released__allocated)
{
    const auto pool = to_shared<server::extranonces>();
    auto set = exhaust(*pool, 1);
    set.clear();
    BC_REQUIRE_EQUAL(pool->size(), 0u);
    BC_REQUIRE(pool->allocate(1));
}

BC_AUTO_TEST_SUITE_END()
