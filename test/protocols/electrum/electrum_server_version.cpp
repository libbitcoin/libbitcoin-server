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

// server.version

static const code invalid_argument{ error::invalid_argument };

BOOST_AUTO_TEST_CASE(electrum__server_version__default__expected)
{
    const auto response = get(R"({"id":0,"method":"server.version","params":["foobar"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("id").as_int64(), 0);
    BOOST_CHECK(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_CHECK_EQUAL(result.size(), 2u);
    BOOST_CHECK(result.at(0).is_string());
    BOOST_CHECK(result.at(1).is_string());
    BOOST_CHECK_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_CHECK_EQUAL(result.at(1).as_string(), "1.4");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__minimum__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.4"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("id").as_int64(), 42);
    BOOST_CHECK(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_CHECK_EQUAL(result.size(), 2u);
    BOOST_CHECK(result.at(0).is_string());
    BOOST_CHECK(result.at(1).is_string());
    BOOST_CHECK_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_CHECK_EQUAL(result.at(1).as_string(), "1.4");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__maximum__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.4.2"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("id").as_int64(), 42);
    BOOST_CHECK(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_CHECK_EQUAL(result.size(), 2u);
    BOOST_CHECK(result.at(0).is_string());
    BOOST_CHECK(result.at(1).is_string());
    BOOST_CHECK_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_CHECK_EQUAL(result.at(1).as_string(), "1.4.2");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__valid_range__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",["1.4","1.4.2"]]})" "\n");
    BOOST_CHECK_EQUAL(response.at("result").as_array().at(1).as_string(), "1.4.2");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__invalid__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","42"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("id").as_int64(), 42);
    BOOST_CHECK(response.at("error").is_object());
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__below_minimum__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.3"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__array_min_exceeds_max__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",["1.4.2","1.4"]]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__array_wrong_size__invalid_argument)
{
    const auto response = get(R"({"id":52,"method":"server.version","params":["foobar",["1.4","1.4.2","extra"]]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__not_strings__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",[1.4,1.4]]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__non_string__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar",1.4]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__subsequent_call__returns_negotiated)
{
    const auto expected = "1.4";
    BOOST_CHECK(handshake(expected));

    const auto response = get(R"({"id":42,"method":"server.version","params":["newname","1.4.2"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("result").as_array().at(1).as_string(), expected);
}

BOOST_AUTO_TEST_CASE(electrum__server_version__subsequent_call_with_invalid_params__success)
{
    const auto expected = "1.4";
    BOOST_CHECK(handshake(expected));

    const auto response = get(R"({"id":57,"method":"server.version","params":["foobar","invalid"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("result").as_array().at(1).as_string(), expected);
}

BOOST_AUTO_TEST_CASE(electrum__server_version__client_name_overflow__invalid_argument)
{
    // Exceeds max_client_name_length (protected).
    const std::string name(1025, 'a');
    const auto request = boost::format(R"({"id":42,"method":"server.version","params":["%1%","1.4"]})" "\n") % name;
    const auto response = get(request.str());
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_SUITE_END()
