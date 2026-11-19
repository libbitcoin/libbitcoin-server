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
#include "../../test.hpp"
#include "electrum_setup_fixture.hpp"

BC_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

using namespace system;
static const code wrong_version{ server::error::electrum::bad_request };
static const code not_implemented{ server::error::electrum::method_not_found };
static const code invalid_argument{ server::error::electrum::bad_request };

// blockchain.estimatefee

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__no_number__invalid_params)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_params);
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__float_number__invalid_argument)
{
    BC_REQUIRE(handshake(electrum::version::v1_0));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42.42]})" "\n");
    BC_REQUIRE_EQUAL(result, invalid_argument.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__mode_invalid_version__invalid_argument)
{
    BC_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"basic"]})" "\n");
    BC_REQUIRE_EQUAL(result, invalid_argument.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__nvalid_mode__invalid_argument)
{
    BC_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"bogus"]})" "\n");
    BC_REQUIRE_EQUAL(result, invalid_argument.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__invalid_mode_v1_6__invalid_argument)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"bogus"]})" "\n");
    BC_REQUIRE_EQUAL(result, invalid_argument.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__basic_empty_pool__zero)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"basic"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BC_REQUIRE_EQUAL(response.at("result").as_double(), 0.0);
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__two_pipelined__ordered_responses)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto first = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"basic"]})" "\n" R"({"id":802,"method":"blockchain.estimatefee","params":[1,"basic"]})" "\n");
    const auto second = receive();
    BC_REQUIRE_MESSAGE(first.is_object() && first.as_object().contains("result"), serialize(first));
    BC_REQUIRE_MESSAGE(second.is_object() && second.as_object().contains("result"), serialize(second));
    BC_REQUIRE_EQUAL(first.at("id").as_int64(), 801);
    BC_REQUIRE_EQUAL(second.at("id").as_int64(), 802);
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__geometric_empty_pool__zero)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"geometric"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BC_REQUIRE_EQUAL(response.at("result").as_double(), 0.0);
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__economical_empty_pool__zero)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"economical"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BC_REQUIRE_EQUAL(response.at("result").as_double(), 0.0);
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__conservative_empty_pool__zero)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"conservative"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BC_REQUIRE_EQUAL(response.at("result").as_double(), 0.0);
}

BC_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__minimum_fee_rate__minimum_fee_rate)
{
    config_.node.pool.minimum_fee_rate = 0.0000152587890625;
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[2]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BC_REQUIRE_EQUAL(response.at("result").as_double(), 1'526.0 / chain::satoshi_per_bitcoin);
}

// blockchain.relayfee

BC_AUTO_TEST_CASE(electrum__blockchain_relay_fee__default__expected)
{
    config_.node.pool.minimum_fee_rate = 42.42;
    BC_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":90,"method":"blockchain.relayfee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 90);
    BC_REQUIRE_EQUAL(response.at("result").as_double(), config_.node.pool.minimum_fee_rate);
}

BC_AUTO_TEST_CASE(electrum__blockchain_relay_fee__obsoleted__wrong_version)
{
    BC_REQUIRE(handshake(electrum::version::v1_6));

    const auto result = get_error(R"({"id":801,"method":"blockchain.relayfee","params":[]})" "\n");
    BC_REQUIRE_EQUAL(result, wrong_version.value());
}

BC_AUTO_TEST_SUITE_END()
