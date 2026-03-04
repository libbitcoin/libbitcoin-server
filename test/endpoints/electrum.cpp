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
#include "electrum.hpp"

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

// server.version

BOOST_AUTO_TEST_CASE(electrum__server_version__default__expected)
{
    const auto response = get(R"({"id":0,"method":"server.version","params":["foobar"]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 0);
    BOOST_REQUIRE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE(result.at(1).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.4");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__minimum__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.4"]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    BOOST_REQUIRE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE(result.at(1).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.4");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__maximum__expected)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.4.2"]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    BOOST_REQUIRE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE(result.at(1).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.4.2");
}

BOOST_AUTO_TEST_CASE(electrum__server_version__invalid__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","42"]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    BOOST_REQUIRE(response.at("error").is_object());

    const code ec{ error::invalid_argument };
    const auto& error = response.at("error").as_object();
    BOOST_REQUIRE_EQUAL(error.size(), 2u);
    BOOST_REQUIRE(error.contains("code"));
    BOOST_REQUIRE_EQUAL(error.at("code").as_int64(), ec.value());
    BOOST_REQUIRE(error.contains("message"));
    BOOST_REQUIRE_EQUAL(std::string(error.at("message").as_string()), ec.message());
}

BOOST_AUTO_TEST_CASE(electrum__server_version__below_minimum__invalid_argument)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar","1.3"]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    BOOST_REQUIRE(response.at("error").is_object());

    const code ec{ error::invalid_argument };
    const auto& error = response.at("error").as_object();
    BOOST_REQUIRE_EQUAL(error.size(), 2u);
    BOOST_REQUIRE(error.contains("code"));
    BOOST_REQUIRE_EQUAL(error.at("code").as_int64(), ec.value());
    BOOST_REQUIRE(error.contains("message"));
    BOOST_REQUIRE_EQUAL(std::string(error.at("message").as_string()), ec.message());
}

// blockchain.block.header

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__genesis__expected)
{
    BOOST_REQUIRE(handshake());
    const auto expected = system::encode_base16(genesis.header().to_data());

    const auto response = get(R"({"id":43,"method":"blockchain.block.header","params":[0]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 43);
    BOOST_REQUIRE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_REQUIRE(result.at("header").is_string());
    BOOST_REQUIRE_EQUAL(result.at("header").as_string(), expected);
}

BOOST_AUTO_TEST_SUITE_END()
