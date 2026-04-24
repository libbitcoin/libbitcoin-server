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

using namespace system;
static const code not_found{ server::error::not_found };
static const code wrong_version{ server::error::wrong_version };
static const code invalid_argument{ server::error::invalid_argument };
static const std::string bogus_address{ "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn" };
static const std::string found_address{ "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM" };

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// blockchain.address.get_balance

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":901,"method":"blockchain.address.get_balance","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto result = get_error(R"({"id":902,"method":"blockchain.address.get_balance","params":[""]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":903,"method":"blockchain.address.get_balance","params":["invalid"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__not_found_address__zero)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":904,"method":"blockchain.address.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_address).str());
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
    const auto response = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("confirmed").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("unconfirmed").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("confirmed").as_int64(), 0x09); // bogus_block10

    // Currently unconfirmed ALWAYS returns zero (no graph to order conflicts).
    BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x00);
    ////BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x10 + 0x11 + 0x12); // bogus_block11
}

// blockchain.address.get_history

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_history__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":1002,"method":"blockchain.address.get_history","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_history__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto result = get_error(R"({"id":1003,"method":"blockchain.address.get_history","params":[""]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_history__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":1004,"method":"blockchain.address.get_history","params":["invalid"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_history__not_found_address__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1005,"method":"blockchain.address.get_history","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_history__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add one confirmed p2sh/p2kh block and one unconfirmed (mirrors the get_balance confirmed test).
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":1006,"method":"blockchain.address.get_history","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& history = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(history.size(), 3u);

    const auto hash1 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto& tx1 = history.at(0).as_object();
    REQUIRE_NO_THROW_TRUE(tx1.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx1.at("tx_hash").is_string());
    BOOST_REQUIRE(!tx1.contains("fee"));
    BOOST_REQUIRE_EQUAL(tx1.at("height").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(tx1.at("tx_hash").as_string(), encode_hash(hash1));

    const auto hash2 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto& tx2 = history.at(1).as_object();
    REQUIRE_NO_THROW_TRUE(tx2.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx2.at("tx_hash").is_string());
    BOOST_REQUIRE_EQUAL(tx2.at("fee").as_int64(), floored_subtract(5'000'000'000 + 5'000'000'000, 0x10 + 0x11 + 0x12 + 0x13 + 0x14));
    BOOST_REQUIRE_EQUAL(tx2.at("height").as_int64(), 0);  // rooted
    BOOST_REQUIRE_EQUAL(tx2.at("tx_hash").as_string(), encode_hash(hash2));

    const auto hash3 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto& tx3 = history.at(2).as_object();
    REQUIRE_NO_THROW_TRUE(tx3.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx3.at("tx_hash").is_string());
    BOOST_REQUIRE_EQUAL(tx3.at("fee").as_int64(), floored_subtract(0x10 + 0x11, 0x0a));
    BOOST_REQUIRE_EQUAL(tx3.at("height").as_int64(), -1); // not rooted
    BOOST_REQUIRE_EQUAL(tx3.at("tx_hash").as_string(), encode_hash(hash3));
}

// blockchain.address.get_mempool

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":1002,"method":"blockchain.address.get_mempool","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto result = get_error(R"({"id":1003,"method":"blockchain.address.get_mempool","params":[""]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":1004,"method":"blockchain.address.get_mempool","params":["invalid"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__not_found_address__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1005,"method":"blockchain.address.get_mempool","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add one confirmed p2sh/p2kh block and one unconfirmed (mirrors the get_balance confirmed test).
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":1006,"method":"blockchain.address.get_mempool","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& history = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(history.size(), 2u);

    const auto hash1 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto& tx1 = history.at(0).as_object();
    REQUIRE_NO_THROW_TRUE(tx1.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx1.at("tx_hash").is_string());
    BOOST_REQUIRE_EQUAL(tx1.at("fee").as_int64(), floored_subtract(5'000'000'000 + 5'000'000'000, 0x10 + 0x11 + 0x12 + 0x13 + 0x14));
    BOOST_REQUIRE_EQUAL(tx1.at("height").as_int64(), 0);  // rooted
    BOOST_REQUIRE_EQUAL(tx1.at("tx_hash").as_string(), encode_hash(hash1));

    const auto hash2 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto& tx2 = history.at(1).as_object();
    REQUIRE_NO_THROW_TRUE(tx2.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx2.at("tx_hash").is_string());
    BOOST_REQUIRE_EQUAL(tx2.at("fee").as_int64(), floored_subtract(0x10 + 0x11, 0x0a));
    BOOST_REQUIRE_EQUAL(tx2.at("height").as_int64(), -1); // not rooted
    BOOST_REQUIRE_EQUAL(tx2.at("tx_hash").as_string(), encode_hash(hash2));
}

// blockchain.address.list_unspent

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":1002,"method":"blockchain.address.listunspent","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto result = get_error(R"({"id":1003,"method":"blockchain.address.listunspent","params":[""]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":1004,"method":"blockchain.address.listunspent","params":["invalid"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__not_found_address__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1005,"method":"blockchain.address.listunspent","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add one confirmed p2sh/p2kh block and one unconfirmed (mirrors the get_balance confirmed test).
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":1006,"method":"blockchain.address.listunspent","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& unspent = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(unspent.size(), 4u);

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto& tx1 = unspent.at(0).as_object();
    BOOST_REQUIRE_EQUAL(tx1.at("tx_hash").as_string(), encode_hash(hash10));
    BOOST_REQUIRE_EQUAL(tx1.at("tx_pos").as_int64(), 0);    // tx.output.index(0)
    BOOST_REQUIRE_EQUAL(tx1.at("height").as_int64(), 10);   // confirmed
    BOOST_REQUIRE_EQUAL(tx1.at("value").as_int64(), 0x09);

    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto& tx2 = unspent.at(1).as_object();
    BOOST_REQUIRE_EQUAL(tx2.at("tx_hash").as_string(), encode_hash(hash11));
    BOOST_REQUIRE_EQUAL(tx2.at("tx_pos").as_int64(), 0);    // tx.output.index(0)
    BOOST_REQUIRE_EQUAL(tx2.at("height").as_int64(), 0);    // unconfirmed
    BOOST_REQUIRE_EQUAL(tx2.at("value").as_int64(), 0x10);

    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto& tx4 = unspent.at(2).as_object();
    BOOST_REQUIRE_EQUAL(tx4.at("tx_hash").as_string(), encode_hash(hash12));
    BOOST_REQUIRE_EQUAL(tx4.at("tx_pos").as_int64(), 0);    // tx.output.index(0)
    BOOST_REQUIRE_EQUAL(tx4.at("height").as_int64(), 0);    // unconfirmed
    BOOST_REQUIRE_EQUAL(tx4.at("value").as_int64(), 0x10);

    const auto& tx3 = unspent.at(3).as_object();
    BOOST_REQUIRE_EQUAL(tx3.at("tx_hash").as_string(), encode_hash(hash11));
    BOOST_REQUIRE_EQUAL(tx3.at("tx_pos").as_int64(), 1);    // tx.output.index(1)
    BOOST_REQUIRE_EQUAL(tx3.at("height").as_int64(), 0);    // unconfirmed
    BOOST_REQUIRE_EQUAL(tx3.at("value").as_int64(), 0x11);

    // Index is point arbitrary sort priority.
    const auto point11_0 = chain::point{ hash11, 0 };
    const auto point11_1 = chain::point{ hash11, 1 };
    const auto point12_0 = chain::point{ hash12, 0 };
    BOOST_REQUIRE(point11_0 < point12_0);
    BOOST_REQUIRE(point12_0 < point11_1);
}

BOOST_AUTO_TEST_SUITE_END()
