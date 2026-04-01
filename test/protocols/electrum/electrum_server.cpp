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
static const code not_found{ server::error::not_found };
static const code target_overflow{ server::error::target_overflow };
static const code invalid_argument{ server::error::invalid_argument };

// server.add_peer

// server.banner

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_no_aparams__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    // params[] required in json 1.0, server drops connection for invalid json-rpc.
    const auto response = get(R"({"id":42,"method":"server.banner"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_named_params__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    // params{} disallowed in json 1.0, server drops connection for invalid json-rpc.
    const auto response = get(R"({"id":42,"method":"server.banner","params":{}})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_empty_params__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":42,"method":"server.banner","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_1__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"1.0","id":42,"method":"server.banner","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_2__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"2.0","id":42,"method":"server.banner"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

// server.donation_address

BOOST_AUTO_TEST_CASE(electrum__server_donation_address__jsonrpc_1__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"1.0","id":43,"method":"server.donation_address","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "donation_address");
}

BOOST_AUTO_TEST_CASE(electrum__server_donation_address__jsonrpc_2__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"2.0","id":43,"method":"server.donation_address"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "donation_address");
}

// server.features
// server.peers.subscribe

// server.ping

BOOST_AUTO_TEST_CASE(electrum__server_ping__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":200,"method":"server.ping","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__server_ping__jsonrpc_unspecified_no_params__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":201,"method":"server.ping"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_ping__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":202,"method":"server.ping","params":["extra"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
