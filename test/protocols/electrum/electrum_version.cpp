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
#include "electrum_setup_fixture.hpp"

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// server.version

static const code invalid_argument{ error::invalid_argument };

BOOST_AUTO_TEST_CASE(electrum__server_version__default__expected)
{
    const auto response = get(R"({"id":0,"method":"server.version","params":["foobar"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 0);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), "server_name");
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.7");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__minimum__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.0"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), "server_name");
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.0");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__maximum__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.7"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), "server_name");
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.7");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__valid_range__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",["1.0","1.7"]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BOOST_REQUIRE_EQUAL(response.at("result").as_array().at(1).as_string(), "1.7");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__invalid__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","42"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__below_minimum__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","0.0"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__array_min_exceeds_max__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",["1.4.2","1.4"]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__array_wrong_size__invalid_argument)
{
    const auto response = get(R"({"id":52,"method":"server.version","params":["foobar",["1.4","1.4.2","extra"]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__not_strings__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",[1.4,1.4]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__non_string__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",1.4]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__subsequent_call__returns_negotiated)
{
    const auto version = electrum::version::v1_4_2;
    const auto expected = electrum::version_to_string(version);
    BOOST_REQUIRE(handshake(version));

    const auto request = R"({"id":42,"method":"server.version","params":["newname","%1%"]})" "\n";
    const auto response = get((boost_format(request) % expected).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BOOST_REQUIRE_EQUAL(response.at("result").as_array().size(), 2u);
    BOOST_REQUIRE(response.at("result").as_array().at(1).is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_array().at(1).as_string(), expected);
}

BOOST_AUTO_TEST_CASE(electrum__server_version__subsequent_call_with_invalid_params__success)
{
    const auto version = electrum::version::v1_4;
    const auto expected = electrum::version_to_string(version);
    BOOST_REQUIRE(handshake(version));

    const auto response = get(R"({"id":57,"method":"server.version","params":["foobar","invalid"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BOOST_REQUIRE_EQUAL(response.at("result").as_array().size(), 2u);
    BOOST_REQUIRE(response.at("result").as_array().at(1).is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_array().at(1).as_string(), expected);
}

BOOST_AUTO_TEST_CASE(electrum__server_version__client_name_overflow__invalid_argument)
{
    // Exceeds max_client_name_length (protected).
    const std::string name(1025, 'a');
    const auto response = get((boost_format(R"({"id":42,"method":"server.version","params":["%1%","1.4"]})" "\n") % name).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

// batch
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(electrum__batch__two_requests__two_ordered_responses)
{
    const auto response = get(
        R"([{"jsonrpc":"2.0","id":1,"method":"server.version","params":["a","1.4"]},)"
        R"({"jsonrpc":"2.0","id":2,"method":"server.version","params":["b","1.4"]}])" "\n");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 2u);
    REQUIRE_NO_THROW_TRUE(batch.at(0).at("id").is_int64());
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(batch.at(1).at("id").as_int64(), 2);
    REQUIRE_NO_THROW_TRUE(batch.at(0).at("result").is_array());
    REQUIRE_NO_THROW_TRUE(batch.at(1).at("result").is_array());
    BOOST_REQUIRE_EQUAL(batch.at(0).at("result").as_array().at(1).as_string(), "1.4");
    BOOST_REQUIRE_EQUAL(batch.at(1).at("result").as_array().at(1).as_string(), "1.4");
}

BOOST_AUTO_TEST_CASE(electrum__batch__single_element__array_of_one)
{
    const auto response = get(
        R"([{"jsonrpc":"2.0","id":7,"method":"server.version","params":["a","1.4"]}])" "\n");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 1u);
    REQUIRE_NO_THROW_TRUE(batch.at(0).at("id").is_int64());
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 7);
    REQUIRE_NO_THROW_TRUE(batch.at(0).at("result").is_array());
}

BOOST_AUTO_TEST_CASE(electrum__batch__result_and_error__both_delivered_in_order)
{
    // The second element depends upon the negotiation by the first.
    const auto response = get(
        R"([{"jsonrpc":"2.0","id":3,"method":"server.version","params":["a","1.4"]},)"
        R"({"jsonrpc":"2.0","id":4,"method":"blockchain.transaction.get","params":["",false]}])" "\n");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 2u);
    REQUIRE_NO_THROW_TRUE(batch.at(0).at("result").is_array());
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 3);
    BOOST_REQUIRE_EQUAL(batch.at(1).at("id").as_int64(), 4);
    REQUIRE_NO_THROW_TRUE(batch.at(1).at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(batch.at(1).at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__batch__failed_negotiation__closed_error_batch)
{
    // Failed first negotiation stops the channel, which closes the open
    // batch response (the second element is abandoned).
    const auto response = get(
        R"([{"jsonrpc":"2.0","id":3,"method":"server.version","params":["a","42"]},)"
        R"({"jsonrpc":"2.0","id":4,"method":"server.version","params":["b","1.4"]}])" "\n");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 1u);
    REQUIRE_NO_THROW_TRUE(batch.at(0).at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 3);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__batch__v1_element__dropped)
{
    // Batch participation of a v1 message (no jsonrpc field) drops.
    const auto response = get(
        R"([{"id":5,"method":"server.version","params":["a","1.4"]}])" "\n");

    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__batch__empty__dropped)
{
    const auto response = get("[]\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
