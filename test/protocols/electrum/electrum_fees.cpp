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

using namespace system;
static const code wrong_version{ server::error::wrong_version };
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };

// blockchain.estimatefee

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__no_number__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__float_number__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[42.42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__mode_invalid_version__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"mode"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__valid__not_implemented)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"mode"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

// blockchain.relayfee

BOOST_AUTO_TEST_CASE(electrum__blockchain_relay_fee__default__expected)
{
    config_.node.minimum_fee_rate = 42.42f;
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":90,"method":"blockchain.relayfee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 90);
    BOOST_REQUIRE_EQUAL(response.at("result").as_double(), config_.node.minimum_fee_rate);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_relay_fee__obsoleted__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.relayfee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_SUITE_END()
