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
#include "../../test.hpp"
#include "electrum.hpp"

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

// blockchain.estimatefee

// blockchain.relay_fee

BOOST_AUTO_TEST_CASE(electrum__blockchain_relay_fee__default__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    constexpr auto expected = 99.0;
    const auto response = get(R"({"id":90,"method":"blockchain.relayfee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 90);
    BOOST_REQUIRE_EQUAL(response.at("result").as_double(), expected);
}

// get_fee_histogram

BOOST_AUTO_TEST_SUITE_END()
