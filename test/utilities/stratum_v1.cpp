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

BC_AUTO_TEST_SUITE(stratum_v1_tests)

using namespace system;

// The target of difficulty one (bits 0x1d00ffff).
static uint256_t difficulty_one() NOEXCEPT
{
    return chain::compact::expand(0x1d00ffff);
}

// A hash of bytes valued by position (0x00..0x1f).
static hash_digest positional() NOEXCEPT
{
    hash_digest out{};
    for (size_t index{}; index < out.size(); ++index)
        out.at(index) = narrow_cast<uint8_t>(index);

    return out;
}

BC_AUTO_TEST_CASE(stratum_v1__to_stratum_hash__positional__words_reversed)
{
    BC_REQUIRE_EQUAL(to_stratum_hash(positional()), "03020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c");
}

BC_AUTO_TEST_CASE(stratum_v1__encode_word__bits__most_significant_first)
{
    BC_REQUIRE_EQUAL(encode_word(0x1d00ffff), "1d00ffff");
}

BC_AUTO_TEST_CASE(stratum_v1__decode_word__bits__expected)
{
    uint32_t value{};
    BC_REQUIRE(decode_word(value, "1d00ffff"));
    BC_REQUIRE_EQUAL(value, 0x1d00ffffu);
}

BC_AUTO_TEST_CASE(stratum_v1__decode_word__short__false)
{
    uint32_t value{};
    BC_REQUIRE(!decode_word(value, "1d00fff"));
}

BC_AUTO_TEST_CASE(stratum_v1__decode_word__not_base16__false)
{
    uint32_t value{};
    BC_REQUIRE(!decode_word(value, "1d00fffz"));
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_target__one__difficulty_one)
{
    BC_REQUIRE(to_share_target(1.0) == difficulty_one());
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_target__two__half)
{
    BC_REQUIRE(to_share_target(2.0) == (difficulty_one() >> 1));
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_target__half__double)
{
    BC_REQUIRE(to_share_target(0.5) == (difficulty_one() << 1));
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_target__zero__saturated)
{
    BC_REQUIRE(to_share_target(0.0) == ~uint256_t{});
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_target__tiny__saturated)
{
    BC_REQUIRE(to_share_target(1.0e-12) == ~uint256_t{});
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_difficulty__difficulty_one__one)
{
    BC_REQUIRE_EQUAL(to_share_difficulty(difficulty_one()), 1.0);
}

BC_AUTO_TEST_CASE(stratum_v1__to_share_difficulty__zero__zero)
{
    BC_REQUIRE_EQUAL(to_share_difficulty(uint256_t{}), 0.0);
}

BC_AUTO_TEST_SUITE_END()
