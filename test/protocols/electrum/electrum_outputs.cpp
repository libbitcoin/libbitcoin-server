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
static const code wrong_version{ server::error::wrong_version };
static const code invalid_argument{ server::error::invalid_argument };

// blockchain.utxo.get_address

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto hash = "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b";
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":903,"method":"blockchain.utxo.get_address","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__invalid_index__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto hash = "4444444444444444444444444444444444444444444444444444444444444444";
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",-1]})" "\n";
    const auto response = get((boost::format(request) % hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":904,"method":"blockchain.utxo.get_address","params":["not_a_hash", 0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__tx_not_found__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto hash = "4444444444444444444444444444444444444444444444444444444444444444";
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__p2pk__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto hash = "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b";
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__p2kh__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add a confirmed p2sh/p2kh block.
    query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false);
    query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), false);

    const auto hash = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    const auto& result = response.at("result").as_string();
    BOOST_REQUIRE_EQUAL(result,"1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM");
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__p2sh__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add a confirmed p2sh/p2kh block.
    query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false);
    query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), false);

    const auto hash = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",1]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    const auto& result = response.at("result").as_string();
    BOOST_REQUIRE_EQUAL(result, "31xsx7sPoS2UfoUAKfoXLX6wTPvpetyo7s");
}

// blockchain.outpoint.get_status
// blockchain.outpoint.subscribe
// blockchain.outpoint.unsubscribe

BOOST_AUTO_TEST_SUITE_END()
