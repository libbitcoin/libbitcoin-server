/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

BOOST_AUTO_TEST_SUITE(electrum_version_tests)

using namespace electrum;
using number = system::config::version;

// version_to_number

BOOST_AUTO_TEST_CASE(electrum_version__version_to_number__all__expected)
{
    BOOST_REQUIRE(version_to_number(version::v0_0)   == number(0, 0));
    BOOST_REQUIRE(version_to_number(version::v0_6)   == number(0, 6));
    BOOST_REQUIRE(version_to_number(version::v0_8)   == number(0, 8));
    BOOST_REQUIRE(version_to_number(version::v0_9)   == number(0, 9));
    BOOST_REQUIRE(version_to_number(version::v0_10)  == number(0, 10));
    BOOST_REQUIRE(version_to_number(version::v1_0)   == number(1, 0));
    BOOST_REQUIRE(version_to_number(version::v1_1)   == number(1, 1));
    BOOST_REQUIRE(version_to_number(version::v1_2)   == number(1, 2));
    BOOST_REQUIRE(version_to_number(version::v1_3)   == number(1, 3));
    BOOST_REQUIRE(version_to_number(version::v1_4)   == number(1, 4));
    BOOST_REQUIRE(version_to_number(version::v1_4_1) == number(1, 4, 1));
    BOOST_REQUIRE(version_to_number(version::v1_4_2) == number(1, 4, 2));
    BOOST_REQUIRE(version_to_number(version::v1_6)   == number(1, 6));
    BOOST_REQUIRE(version_to_number(version::v1_7)   == number(1, 7));
}

// version_to_string

BOOST_AUTO_TEST_CASE(electrum_version__version_to_string__all__expected)
{
    BOOST_REQUIRE_EQUAL(version_to_string(version::v0_0), "0.0");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v0_6), "0.6");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v0_8), "0.8");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v0_9), "0.9");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v0_10), "0.10");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_0), "1.0");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_1), "1.1");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_2), "1.2");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_3), "1.3");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_4), "1.4");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_4_1), "1.4.1");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_4_2), "1.4.2");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_6), "1.6");
    BOOST_REQUIRE_EQUAL(version_to_string(version::v1_7), "1.7");
}

// version_from_string

BOOST_AUTO_TEST_CASE(electrum_version__version_from_string__defined__expected)
{
    number out{};
    BOOST_REQUIRE(version_from_string(out, "0.0"));
    BOOST_REQUIRE(out == number(0, 0));
    BOOST_REQUIRE(version_from_string(out, "0.10"));
    BOOST_REQUIRE(out == number(0, 10));
    BOOST_REQUIRE(version_from_string(out, "1.4.2"));
    BOOST_REQUIRE(out == number(1, 4, 2));
    BOOST_REQUIRE(version_from_string(out, "1.7"));
    BOOST_REQUIRE(out == number(1, 7));
}

BOOST_AUTO_TEST_CASE(electrum_version__version_from_string__undefined__expected)
{
    number out{};
    BOOST_REQUIRE(version_from_string(out, "0.7"));
    BOOST_REQUIRE(out == number(0, 7));
    BOOST_REQUIRE(version_from_string(out, "1.5"));
    BOOST_REQUIRE(out == number(1, 5));
    BOOST_REQUIRE(version_from_string(out, "1.8"));
    BOOST_REQUIRE(out == number(1, 8));
    BOOST_REQUIRE(version_from_string(out, "2.0"));
    BOOST_REQUIRE(out == number(2, 0));
    BOOST_REQUIRE(version_from_string(out, "42.24.7.11"));
    BOOST_REQUIRE(out == number(42, 24, 7, 11));
}

BOOST_AUTO_TEST_CASE(electrum_version__version_from_string__malformed__false)
{
    number out{};
    BOOST_REQUIRE(!version_from_string(out, ""));
    BOOST_REQUIRE(!version_from_string(out, "42"));
    BOOST_REQUIRE(!version_from_string(out, "1."));
    BOOST_REQUIRE(!version_from_string(out, "1..4"));
    BOOST_REQUIRE(!version_from_string(out, "1.x"));
    BOOST_REQUIRE(!version_from_string(out, "1.4x"));
    BOOST_REQUIRE(!version_from_string(out, "1.4.2.0.0"));
    BOOST_REQUIRE(!version_from_string(out, "1.-4"));
    BOOST_REQUIRE(!version_from_string(out, "1.4 junk"));
    BOOST_REQUIRE(!version_from_string(out, " 1.4"));
}

// version_floor

BOOST_AUTO_TEST_CASE(electrum_version__version_floor__defined__same)
{
    BOOST_REQUIRE(version_floor(number(0, 0)) == version::v0_0);
    BOOST_REQUIRE(version_floor(number(0, 6)) == version::v0_6);
    BOOST_REQUIRE(version_floor(number(1, 0)) == version::v1_0);
    BOOST_REQUIRE(version_floor(number(1, 4, 1)) == version::v1_4_1);
    BOOST_REQUIRE(version_floor(number(1, 4, 2)) == version::v1_4_2);
    BOOST_REQUIRE(version_floor(number(1, 7)) == version::v1_7);
}

BOOST_AUTO_TEST_CASE(electrum_version__version_floor__undefined__next_lower)
{
    BOOST_REQUIRE(version_floor(number(0, 5)) == version::v0_0);
    BOOST_REQUIRE(version_floor(number(0, 7)) == version::v0_6);
    BOOST_REQUIRE(version_floor(number(1, 4, 3)) == version::v1_4_2);
    BOOST_REQUIRE(version_floor(number(1, 5)) == version::v1_4_2);
    BOOST_REQUIRE(version_floor(number(1, 8)) == version::v1_7);
    BOOST_REQUIRE(version_floor(number(2, 0)) == version::v1_7);
}

BOOST_AUTO_TEST_SUITE_END()
