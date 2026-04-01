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

BOOST_AUTO_TEST_CASE(electrum_version__version_from_string__all__expected)
{
    BOOST_REQUIRE(version_from_string("0.0") == version::v0_0);
    BOOST_REQUIRE(version_from_string("0.6") == version::v0_6);
    BOOST_REQUIRE(version_from_string("0.8") == version::v0_8);
    BOOST_REQUIRE(version_from_string("0.9") == version::v0_9);
    BOOST_REQUIRE(version_from_string("0.10") == version::v0_10);
    BOOST_REQUIRE(version_from_string("1.0") == version::v1_0);
    BOOST_REQUIRE(version_from_string("1.1") == version::v1_1);
    BOOST_REQUIRE(version_from_string("1.2") == version::v1_2);
    BOOST_REQUIRE(version_from_string("1.3") == version::v1_3);
    BOOST_REQUIRE(version_from_string("1.4") == version::v1_4);
    BOOST_REQUIRE(version_from_string("1.4.1") == version::v1_4_1);
    BOOST_REQUIRE(version_from_string("1.4.2") == version::v1_4_2);
    BOOST_REQUIRE(version_from_string("1.6") == version::v1_6);
    BOOST_REQUIRE(version_from_string("1.7") == version::v1_7);
}

BOOST_AUTO_TEST_SUITE_END()
