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

using namespace system;
static const code not_found{ server::error::not_found };
static const code wrong_version{ server::error::wrong_version };
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };

BOOST_FIXTURE_TEST_SUITE(electrum_disabled_address_tests, electrum_disabled_address_index_setup_fixture)

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":901,"method":"blockchain.address.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn").str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// blockchain.address.get_balance

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":901,"method":"blockchain.address.get_balance","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__empty_v1_3__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto response = get(R"({"id":902,"method":"blockchain.address.get_balance","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":903,"method":"blockchain.address.get_balance","params":["invalid"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__not_found_address__zero)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":904,"method":"blockchain.address.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn").str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("confirmed").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("unconfirmed").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("confirmed").as_int64(), 0x00);
    BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x00);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add one confirmed p2sh/p2kh block and one unconfirmed.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":905,"method":"blockchain.address.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM").str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("confirmed").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("unconfirmed").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("confirmed").as_int64(), 0x09); // bogus_block11

    // Currently unconfirmed ALWAYS returns zero (no graph to order conflicts).
    BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x00);
    ////BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x10 + 0x11 + 0x12); // bogus_block11
}

// blockchain.address.get_history
// blockchain.address.get_mempool
// blockchain.address.list_unspent
// blockchain.address.subscribe

BOOST_AUTO_TEST_SUITE_END()
