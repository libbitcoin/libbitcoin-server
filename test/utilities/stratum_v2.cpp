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

BC_AUTO_TEST_SUITE(stratum_v2_utility_tests)

using namespace system;

// The target of difficulty one (bits 0x1d00ffff).
static hash_digest difficulty_one() NOEXCEPT
{
    return from_uintx(chain::compact::expand(0x1d00ffff));
}

BC_AUTO_TEST_CASE(stratum_v2__to_target_hash__one__difficulty_one_little_endian)
{
    BC_REQUIRE_EQUAL(server::to_target_hash(1.0), difficulty_one());
    BC_REQUIRE_EQUAL(server::to_target_hash(1.0).back(), 0x00u);
    BC_REQUIRE_EQUAL(server::to_target_hash(1.0).at(26), 0xffu);
}

BC_AUTO_TEST_CASE(stratum_v2__to_target_hash__two__half_target)
{
    BC_REQUIRE_EQUAL(to_uintx(server::to_target_hash(2.0)), to_uintx(difficulty_one()) >> 1);
}

BC_AUTO_TEST_CASE(stratum_v2__to_hash_difficulty__difficulty_one__one)
{
    BC_REQUIRE_EQUAL(server::to_hash_difficulty(difficulty_one()), 1.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_hash_difficulty__round_trip__expected)
{
    BC_REQUIRE_EQUAL(server::to_hash_difficulty(server::to_target_hash(1024.0)), 1024.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_hash_difficulty__null_hash__zero)
{
    BC_REQUIRE_EQUAL(server::to_hash_difficulty(null_hash), 0.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__two_pow_32_per_second__one)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(4294967296.0, 1), 1.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__interval__scaled)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(4294967296.0, 10), 10.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__zero_rate__zero)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(0.0, 10), 0.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__negative_rate__zero)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(-1.0, 10), 0.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__nan_rate__zero)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(std::numeric_limits<double>::quiet_NaN(), 10), 0.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__infinite_rate__zero)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(std::numeric_limits<double>::infinity(), 10), 0.0);
}

BC_AUTO_TEST_CASE(stratum_v2__to_rate_difficulty__zero_interval__zero)
{
    BC_REQUIRE_EQUAL(server::to_rate_difficulty(4294967296.0, 0), 0.0);
}

BC_AUTO_TEST_SUITE_END()
