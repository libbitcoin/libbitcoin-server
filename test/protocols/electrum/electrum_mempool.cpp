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

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

using namespace system;
static const code wrong_version{ server::error::wrong_version };
static const code not_implemented{ server::error::not_implemented };

// mempool.get_fee_histogram

BOOST_AUTO_TEST_CASE(electrum__mempool_get_fee_histogram__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":600,"method":"mempool.get_fee_histogram","params":[]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__mempool_get_fee_histogram__no_params_key__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":601,"method":"mempool.get_fee_histogram"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__mempool_get_fee_histogram__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":602,"method":"mempool.get_fee_histogram","params":[42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__mempool_get_fee_histogram__empty_params__not_implemented)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto result = get_error(R"({"id":603,"method":"mempool.get_fee_histogram","params":[]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

// mempool.get_info

BOOST_AUTO_TEST_CASE(electrum__mempool_get_info__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":700,"method":"mempool.get_info","params":[]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__mempool_get_info__no_params_key__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":701,"method":"mempool.get_info"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__mempool_get_info__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":702,"method":"mempool.get_info","params":[42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__mempool_get_info__empty_params__expected)
{
    config_.node.minimum_fee_rate = 42.42f;
    config_.node.minimum_bump_rate = 24.24f;
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":703,"method":"mempool.get_info","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("mempoolminfee").is_number());
    REQUIRE_NO_THROW_TRUE(result.at("minrelaytxfee").is_number());
    REQUIRE_NO_THROW_TRUE(result.at("incrementalrelayfee").is_number());
    BOOST_REQUIRE_EQUAL(result.at("mempoolminfee").as_double(), config_.node.minimum_fee_rate);
    BOOST_REQUIRE_EQUAL(result.at("minrelaytxfee").as_double(), config_.node.minimum_fee_rate);
    BOOST_REQUIRE_EQUAL(result.at("incrementalrelayfee").as_double(), config_.node.minimum_bump_rate);
}

BOOST_AUTO_TEST_SUITE_END()
