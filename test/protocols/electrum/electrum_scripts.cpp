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
static const chain::script bogus{ chain::script::to_pay_key_hash_pattern({ 0x42 }) };
static const chain::script found{ chain::script::to_pay_key_hash_pattern({ 0x02 }) };
static const auto bogus_script = encode_base16(bogus.to_data(false));
static const auto found_script = encode_base16(found.to_data(false));

BOOST_FIXTURE_TEST_SUITE(electrum_disabled_address_tests, electrum_disabled_address_index_setup_fixture)

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":901,"method":"blockchain.scriptpubkey.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.get_history","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.get_mempool","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.listunspent","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// blockchain.scriptpubkey.get_balance

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":901,"method":"blockchain.scriptpubkey.get_balance","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__insufficient_version__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":902,"method":"blockchain.scriptpubkey.get_balance","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

// Not yet.
////BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__obsoleted_version__wrong_version)
////{
////    BOOST_REQUIRE(handshake(electrum::version::v1_8));
////
////    const auto response = get(R"({"id":902,"method":"blockchain.scriptpubkey.get_balance","params":[""]})" "\n");
////    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
////    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
////}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":903,"method":"blockchain.scriptpubkey.get_balance","params":["invalid"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__not_found_address__zero)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":904,"method":"blockchain.scriptpubkey.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("confirmed").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("unconfirmed").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("confirmed").as_int64(), 0x00);
    BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x00);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // Add one confirmed p2sh/p2kh block and one unconfirmed.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":905,"method":"blockchain.scriptpubkey.get_balance","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("confirmed").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("unconfirmed").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("confirmed").as_int64(), 0x09); // bogus_block10

    // Currently unconfirmed ALWAYS returns zero (no graph to order conflicts).
    BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x00);
    ////BOOST_REQUIRE_EQUAL(result.at("unconfirmed").as_int64(), 0x10 + 0x11 + 0x12); // bogus_block11
}

// blockchain.scriptpubkey.get_history

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1002,"method":"blockchain.scriptpubkey.get_history","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__insufficient_version__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":902,"method":"blockchain.scriptpubkey.get_history","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

// Not yet.
////BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__obsoleted_version__wrong_version)
////{
////    BOOST_REQUIRE(handshake(electrum::version::v1_8));
////
////    const auto response = get(R"({"id":1003,"method":"blockchain.scriptpubkey.get_history","params":[""]})" "\n");
////    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
////    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
////}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1004,"method":"blockchain.scriptpubkey.get_history","params":["invalid"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__not_found_address__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1005,"method":"blockchain.scriptpubkey.get_history","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // Add one confirmed p2sh/p2kh block and one unconfirmed (mirrors the get_balance confirmed test).
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":1006,"method":"blockchain.scriptpubkey.get_history","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_script).str());
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
    BOOST_REQUIRE_EQUAL(tx2.at("fee").as_int64(), floored_subtract(5'000'000'000 + 5'000'000'000, 0x10 + 0x11 + 0x12 + 0x13 + 0x14));
    BOOST_REQUIRE_EQUAL(tx3.at("height").as_int64(), -1); // not rooted
    BOOST_REQUIRE_EQUAL(tx3.at("tx_hash").as_string(), encode_hash(hash3));
}

// blockchain.scriptpubkey.get_mempool

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1002,"method":"blockchain.scriptpubkey.get_mempool","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__insufficient_version__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":902,"method":"blockchain.scriptpubkey.get_mempool","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

// Not yet.
////BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__obsoleted_version__wrong_version)
////{
////    BOOST_REQUIRE(handshake(electrum::version::v1_8));
////
////    const auto response = get(R"({"id":1003,"method":"blockchain.scriptpubkey.get_mempool","params":[""]})" "\n");
////    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
////    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
////}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1004,"method":"blockchain.scriptpubkey.get_mempool","params":["invalid"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__not_found_address__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1005,"method":"blockchain.scriptpubkey.get_mempool","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // Add one confirmed p2sh/p2kh block and one unconfirmed (mirrors the get_balance confirmed test).
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":1006,"method":"blockchain.scriptpubkey.get_mempool","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& history = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(history.size(), 2u);

    const auto hash1 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto& tx1 = history.at(0).as_object();
    REQUIRE_NO_THROW_TRUE(tx1.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx1.at("tx_hash").is_string());
    BOOST_REQUIRE_EQUAL(tx1.at("fee").as_int64(), floored_subtract(5'000'000'000 + 5'000'000'000, 0x10 + 0x11 + 0x12 + 0x13 + 0x14));
    BOOST_REQUIRE_EQUAL(tx1.at("tx_hash").as_string(), encode_hash(hash1));

    const auto hash2 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto& tx2 = history.at(1).as_object();
    REQUIRE_NO_THROW_TRUE(tx2.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(tx2.at("tx_hash").is_string());
    BOOST_REQUIRE_EQUAL(tx2.at("fee").as_int64(), floored_subtract(0x10 + 0x11, 0x0a));
    BOOST_REQUIRE_EQUAL(tx2.at("height").as_int64(), -1); // not rooted
    BOOST_REQUIRE_EQUAL(tx2.at("tx_hash").as_string(), encode_hash(hash2));
}

// blockchain.scriptpubkey.list_unspent

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__missing_arguments__dropped)
{
    BOOST_REQUIRE(query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1002,"method":"blockchain.scriptpubkey.listunspent","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__insufficient_version__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":902,"method":"blockchain.scriptpubkey.listunspent","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

// Not yet.
////BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__obsoleted_version__wrong_version)
////{
////    BOOST_REQUIRE(handshake(electrum::version::v1_8));
////
////    const auto response = get(R"({"id":1003,"method":"blockchain.scriptpubkey.listunspent","params":[""]})" "\n");
////    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
////    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
////}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__invalid_address__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1004,"method":"blockchain.scriptpubkey.listunspent","params":["invalid"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__not_found_address__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1005,"method":"blockchain.scriptpubkey.listunspent","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__confirmed_and_unconfirmed_address__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // Add one confirmed p2sh/p2kh block and one unconfirmed (mirrors the get_balance confirmed test).
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":1006,"method":"blockchain.scriptpubkey.listunspent","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_script).str());
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

// blockchain.scriptpubkey.subscribe
// blockchain.scriptpubkey.unsubscribe

BOOST_AUTO_TEST_SUITE_END()
