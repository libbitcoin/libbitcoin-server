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
#include "../electrum/electrum_setup_fixture.hpp"

using namespace system;

static const code not_implemented{ server::error::electrum::method_not_found };

BOOST_FIXTURE_TEST_SUITE(sparrow_tests, sparrow_ten_block_setup_fixture)

// inherited electrum interface

BOOST_AUTO_TEST_CASE(sparrow__handshake__electrum_version__negotiated)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_numblocks_subscribe__ten_block_store__returns_9)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":900,"method":"blockchain.numblocks.subscribe","params":[]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(sparrow__unknown_method__electrum_terminal__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":901,"method":"server.bogus","params":[]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

// server.features

BOOST_AUTO_TEST_CASE(sparrow__server_features__silent_payments__advertised)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":902,"method":"server.features","params":[]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("silent_payments").is_array());

    const auto& versions = result.at("silent_payments").as_array();
    BOOST_REQUIRE_EQUAL(versions.size(), one);
    BOOST_REQUIRE_EQUAL(versions.at(0).as_int64(), server::protocol_sparrow::silent_payments_version);
    BOOST_REQUIRE_EQUAL(result.at("server_version").as_string(), "server_name");
}

// sparrow interface (stubs)

BOOST_AUTO_TEST_CASE(sparrow__blockchain_block_stats__stub__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":903,"method":"blockchain.block.stats","params":[5]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_silentpayments_subscribe__stub__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto request =
        R"({"id":904,"method":"blockchain.silentpayments.subscribe","params":[")"
        R"(0000000000000000000000000000000000000000000000000000000000000001",")"
        R"(020000000000000000000000000000000000000000000000000000000000000002"]})" "\n";

    BOOST_REQUIRE_EQUAL(get_error(request), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_silentpayments_unsubscribe__stub__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto request =
        R"({"id":905,"method":"blockchain.silentpayments.unsubscribe","params":[")"
        R"(0000000000000000000000000000000000000000000000000000000000000001",")"
        R"(020000000000000000000000000000000000000000000000000000000000000002"]})" "\n";

    BOOST_REQUIRE_EQUAL(get_error(request), not_implemented.value());
}

BOOST_AUTO_TEST_SUITE_END()

// electrum does not serve the sparrow interface

BOOST_FIXTURE_TEST_SUITE(sparrow_electrum_tests, electrum_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_stats__not_sparrow__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":906,"method":"blockchain.block.stats","params":[5]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__not_sparrow__no_silent_payments)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":907,"method":"server.features","params":[]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE(!response.at("result").as_object().contains("silent_payments"));
}

BOOST_AUTO_TEST_SUITE_END()
