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

using namespace system;
static const code not_found{ server::error::not_found };
static const code wrong_version{ server::error::wrong_version };
static const code invalid_argument{ server::error::invalid_argument };
static const std::string found_address{ "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM" };
static const std::string bogus_hash{ "4242424242424242424242424242424242424242424242424242424242424242" };

// blockchain.utxo.get_address

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
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

    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",-1]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto result = get_error(R"({"id":904,"method":"blockchain.utxo.get_address","params":["not_a_hash", 0]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__tx_not_found__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__p2pk__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__p2kh__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add a confirmed p2sh/p2kh block.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), false));

    const auto hash = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    const auto& result = response.at("result").as_string();
    BOOST_REQUIRE_EQUAL(result, found_address);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_utxo_get_address__p2sh__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // Add a confirmed p2sh/p2kh block.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), false));

    const auto hash = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto request = R"({"id":901,"method":"blockchain.utxo.get_address","params":["%1%",1]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    const auto& result = response.at("result").as_string();
    BOOST_REQUIRE_EQUAL(result, "31xsx7sPoS2UfoUAKfoXLX6wTPvpetyo7s");
}

// blockchain.outpoint.get_status

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto request = R"({"id":1101,"method":"blockchain.outpoint.get_status","params":["%1%",0]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1102,"method":"blockchain.outpoint.get_status","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.outpoint.get_status","params":["not_a_hash", 0]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__invalid_index__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.get_status","params":["%1%",-1]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__tx_not_found__empty_object)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1105,"method":"blockchain.outpoint.get_status","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_object().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__confirmed_unspent__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto hash = test::block1.transactions_ptr()->at(0)->hash(false);
    const auto request = R"({"id":1106,"method":"blockchain.outpoint.get_status","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
    BOOST_REQUIRE(!result.contains("spender_txhash"));
    BOOST_REQUIRE(!result.contains("spender_height"));
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_get_status__confirmed_spent__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash1 = test::block1.transactions_ptr()->at(0)->hash(false);
    const auto request = R"({"id":1107,"method":"blockchain.outpoint.get_status","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash1)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("spender_height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("spender_txhash").is_string());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("spender_height").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(result.at("spender_txhash").as_string(), encode_hash(hash10));
}

// blockchain.outpoint.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto request = R"({"id":1101,"method":"blockchain.outpoint.subscribe","params":["%1%",0]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1102,"method":"blockchain.outpoint.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.outpoint.subscribe","params":["not_a_hash", 0]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__invalid_index__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.subscribe","params":["%1%",-1]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__invalid_hint_encoding__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.subscribe","params":["%1%",-1,"not_hex"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__invalid_hint__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.subscribe","params":["%1%",-1,""]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.subscribe","params":["%1%",-1,"00",42]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__tx_not_found__empty_object)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1105,"method":"blockchain.outpoint.subscribe","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_object().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__confirmed_unspent__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto hash = test::block1.transactions_ptr()->at(0)->hash(false);
    const auto request = R"({"id":1106,"method":"blockchain.outpoint.subscribe","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
    BOOST_REQUIRE(!result.contains("spender_txhash"));
    BOOST_REQUIRE(!result.contains("spender_height"));
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__confirmed_spent__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash1 = test::block1.transactions_ptr()->at(0)->hash(false);
    const auto request = R"({"id":1107,"method":"blockchain.outpoint.subscribe","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(hash1)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("spender_height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("spender_txhash").is_string());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("spender_height").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(result.at("spender_txhash").as_string(), encode_hash(hash10));
}

// TODO: retest after buffering to preclude concurrent socket write.
////BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_subscribe__multiple_spenders__notifications)
////{
////    BOOST_REQUIRE(handshake(electrum::version::v1_7));
////
////    BOOST_REQUIRE(query_.set(test::block1a, database::context{ 0, 1, 0 }, false, false));
////    BOOST_REQUIRE(query_.set(test::block2a, database::context{ 0, 2, 0 }, false, false));
////    BOOST_REQUIRE(query_.set(test::block3a, database::context{ 0, 3, 0 }, false, false));
////    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::block1a.hash()), true));
////    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::block2a.hash()), true));
////    const auto hash1 = encode_hash(test::block1a.transactions_ptr()->at(0)->hash(false));
////    const auto hash2 = encode_hash(test::block2a.transactions_ptr()->at(0)->hash(false));
////    const auto hash3 = encode_hash(test::block3a.transactions_ptr()->at(0)->hash(false));
////
////    // block1a tx0 output0 [confirmed 1]
////    const auto request = R"({"id":1107,"method":"blockchain.outpoint.subscribe","params":["%1%",0]})" "\n";
////    const auto response = get((boost::format(request) % hash1).str());
////    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
////
////    // spent by block2a tx0 input0 [confirmed 2]
////    // spent by block3a tx0 input1 [unconfirmed]
////    const auto& result = response.at("result").as_object();
////    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
////    REQUIRE_NO_THROW_TRUE(result.at("spender_height").is_int64());
////    REQUIRE_NO_THROW_TRUE(result.at("spender_txhash").is_string());
////    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
////    BOOST_REQUIRE_EQUAL(result.at("spender_height").as_int64(), 2);
////    BOOST_REQUIRE_EQUAL(result.at("spender_txhash").as_string(), hash2);
////
////    const auto notification = receive();
////    REQUIRE_NO_THROW_TRUE(notification.at("method").is_string());
////    REQUIRE_NO_THROW_TRUE(notification.at("params").is_array());
////    BOOST_REQUIRE_EQUAL(notification.at("method").as_string(), "blockchain.outpoint.subscribe");
////
////    const auto& params = notification.at("params").as_array();
////    BOOST_REQUIRE_EQUAL(params.size(), 2u);
////    BOOST_REQUIRE(params.at(0).is_array());
////    BOOST_REQUIRE(params.at(1).is_object());
////
////    const auto& outpoint = params.at(0).as_array();
////    BOOST_REQUIRE_EQUAL(outpoint.size(), 2u);
////    BOOST_REQUIRE(outpoint.at(0).is_string());
////    BOOST_REQUIRE(outpoint.at(1).is_number());
////
////    const auto& spender = params.at(1).as_object();
////    REQUIRE_NO_THROW_TRUE(spender.at("height").is_int64());
////    REQUIRE_NO_THROW_TRUE(spender.at("spender_height").is_int64());
////    REQUIRE_NO_THROW_TRUE(spender.at("spender_txhash").is_string());
////    BOOST_CHECK_EQUAL(spender.at("height").as_int64(), 1);
////    BOOST_CHECK_EQUAL(spender.at("spender_height").as_int64(), 3);
////    BOOST_CHECK_EQUAL(spender.at("spender_txhash").as_string(), hash3);
////}

// blockchain.outpoint.unsubscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto request = R"({"id":1101,"method":"blockchain.outpoint.unsubscribe","params":["%1%",0]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1102,"method":"blockchain.outpoint.unsubscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.outpoint.unsubscribe","params":["4242", 0]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__invalid_index__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.unsubscribe","params":["%1%",-1]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_hash).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.outpoint.unsubscribe","params":["%1%",-1,""]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__unsubscribed__false)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1101,"method":"blockchain.outpoint.unsubscribe","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % bogus_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_bool());
    BOOST_REQUIRE(!response.at("result").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_outpoint_unsubscribe__subscribed__true)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto hash1 = encode_hash(test::block1.transactions_ptr()->at(0)->hash(false));
    const auto request1 = R"({"id":1107,"method":"blockchain.outpoint.subscribe","params":["%1%",0]})" "\n";
    const auto response1 = get((boost::format(request1) % hash1).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_object());

    const auto request2 = R"({"id":1101,"method":"blockchain.outpoint.unsubscribe","params":["%1%",0]})" "\n";
    const auto response2 = get((boost::format(request2) % hash1).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
