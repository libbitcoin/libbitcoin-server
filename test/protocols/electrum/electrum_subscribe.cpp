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
static const code invalid_argument{ server::error::invalid_argument };
static const std::string bogus_address{ "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn" };
static const std::string found_address{ "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM" };
static const std::string bogus_scripthash{ "9c2c84a6cf9809e08af19557e28d38257e6fee6981269760637a5f9dfb000b05" };
static const std::string found_scripthash{ "bad83872c90886be19b98734fd16741611efcd9f5de699c14b712675eec682f5" };
static const chain::script bogus{ chain::script::to_pay_key_hash_pattern({ 0x42 }) };
static const chain::script found{ chain::script::to_pay_key_hash_pattern({ 0x02 }) };
static const auto bogus_script = encode_base16(bogus.to_data(false));
static const auto found_script = encode_base16(found.to_data(false));

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// blockchain.address.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__obsolete_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto request = R"({"id":1101,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_address).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":1102,"method":"blockchain.address.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.address.subscribe","params":["not_a_hash"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":1104,"method":"blockchain.address.subscribe","params":["%1%",42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__bogus_p2pkh__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1101,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__initialization__expected_status)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // This validates the hash accumulator copy in get_scripthash_history() and incorporates
    // confirmed, rooted and unrooted transactions, duplicates, and sort.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto expected_initial = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), expected_initial);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__repeat_call__idempotent)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto expected_initial = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_initial);

    const auto response2 = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response2.at("result").as_string(), expected_initial);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__progressive__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);

    // Confirming block 10 also makes block 11 to rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    const auto expected_confirm10 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_confirm10);

    // Confirming block 11 also makes block 12 rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    const auto expected_confirm11 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":0:"
    ));

    const auto response2 = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response2.at("result").as_string(), expected_confirm11);

    // Confirming block 12 only makes block 12 confirmed.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block12.hash()), true));
    const auto expected_confirm12 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":12:"
    ));

    const auto response3 = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response3.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response3.at("result").as_string(), expected_confirm12);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__progressive_notify__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);

    // Confirming block 10 also makes block 11 to rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    const auto expected_confirm10 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_confirm10);

    // Confirming block 11 also makes block 12 rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    const auto expected_confirm11 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":0:"
    ));

    // Trigger node chaser event to electrum event subscriber.
    notify(node::chase::organized, {});

    const auto notification1 = receive();
    REQUIRE_NO_THROW_TRUE(notification1.at("method").is_string());
    REQUIRE_NO_THROW_TRUE(notification1.at("params").is_array());
    BOOST_CHECK_EQUAL(notification1.at("method").as_string(), "blockchain.address.subscribe");

    const auto& params1 = notification1.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params1.size(), 2u);
    BOOST_CHECK(params1.at(0).is_string());
    BOOST_CHECK(params1.at(1).is_string());
    BOOST_CHECK_EQUAL(params1.at(0).as_string(), found_scripthash);
    BOOST_CHECK_EQUAL(params1.at(1).as_string(), expected_confirm11);

    // Confirming block 12 only makes block 12 confirmed.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block12.hash()), true));
    const auto expected_confirm12 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":12:"
    ));

    // Trigger node chaser event to electrum event subscriber.
    notify(node::chase::organized, {});

    const auto notification2 = receive();
    REQUIRE_NO_THROW_TRUE(notification2.at("method").is_string());
    REQUIRE_NO_THROW_TRUE(notification2.at("params").is_array());
    BOOST_CHECK_EQUAL(notification2.at("method").as_string(), "blockchain.address.subscribe");

    const auto& params2 = notification2.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params2.size(), 2u);
    BOOST_CHECK(params2.at(0).is_string());
    BOOST_CHECK(params2.at(1).is_string());
    BOOST_CHECK_EQUAL(params2.at(0).as_string(), found_scripthash);
    BOOST_CHECK_EQUAL(params2.at(1).as_string(), expected_confirm12);
}

// blockchain.scripthash.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":1102,"method":"blockchain.scripthash.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.scripthash.subscribe","params":["not_a_hash"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":1104,"method":"blockchain.scripthash.subscribe","params":["%1%",42]})" "\n";
    const auto response = get((boost::format(request) % bogus_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__bogus_scripthash__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__initialization__expected_status)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    // This validates the hash accumulator copy in get_scripthash_history() and incorporates
    // confirmed, rooted and unrooted transactions, duplicates, and sort.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto expected_initial = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), expected_initial);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__repeat_call__idempotent)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto expected_initial = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_initial);

    const auto response2 = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response2.at("result").as_string(), expected_initial);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__progressive__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);

    // Confirming block 10 also makes block 11 to rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    const auto expected_confirm10 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_confirm10);

    // Confirming block 11 also makes block 12 rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    const auto expected_confirm11 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":0:"
    ));

    const auto response2 = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response2.at("result").as_string(), expected_confirm11);

    // Confirming block 12 only makes block 12 confirmed.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block12.hash()), true));
    const auto expected_confirm12 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":12:"
    ));

    const auto response3 = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response3.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response3.at("result").as_string(), expected_confirm12);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__progressive_notify__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);

    // Confirming block 10 also makes block 11 to rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    const auto expected_confirm10 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_confirm10);

    // Confirming block 11 also makes block 12 rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    const auto expected_confirm11 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":0:"
    ));

    // Trigger node chaser event to electrum event subscriber.
    notify(node::chase::organized, {});

    const auto notification1 = receive();
    REQUIRE_NO_THROW_TRUE(notification1.at("method").is_string());
    REQUIRE_NO_THROW_TRUE(notification1.at("params").is_array());
    BOOST_CHECK_EQUAL(notification1.at("method").as_string(), "blockchain.scripthash.subscribe");

    const auto& params1 = notification1.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params1.size(), 2u);
    BOOST_CHECK(params1.at(0).is_string());
    BOOST_CHECK(params1.at(1).is_string());
    BOOST_CHECK_EQUAL(params1.at(0).as_string(), found_scripthash);
    BOOST_CHECK_EQUAL(params1.at(1).as_string(), expected_confirm11);

    // Confirming block 12 only makes block 12 confirmed.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block12.hash()), true));
    const auto expected_confirm12 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":12:"
    ));

    // Trigger node chaser event to electrum event subscriber.
    notify(node::chase::organized, {});

    const auto notification2 = receive();
    REQUIRE_NO_THROW_TRUE(notification2.at("method").is_string());
    REQUIRE_NO_THROW_TRUE(notification2.at("params").is_array());
    BOOST_CHECK_EQUAL(notification2.at("method").as_string(), "blockchain.scripthash.subscribe");

    const auto& params2 = notification2.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params2.size(), 2u);
    BOOST_CHECK(params2.at(0).is_string());
    BOOST_CHECK(params2.at(1).is_string());
    BOOST_CHECK_EQUAL(params2.at(0).as_string(), found_scripthash);
    BOOST_CHECK_EQUAL(params2.at(1).as_string(), expected_confirm12);
}


// blockchain.scripthash.unsubscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_unsubscribe__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4_1));

    const auto request = R"({"id":1101,"method":"blockchain.scripthash.unsubscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_unsubscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4_2));

    const auto response = get(R"({"id":1102,"method":"blockchain.scripthash.unsubscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_unsubscribe__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4_2));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.scripthash.unsubscribe","params":["4242"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_unsubscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4_2));

    const auto request = R"({"id":1104,"method":"blockchain.scripthash.unsubscribe","params":["%1%",-1]})" "\n";
    const auto response = get((boost::format(request) % bogus_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

// blockchain.scriptpubkey.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1102,"method":"blockchain.scriptpubkey.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__invalid_script_encoding__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.scriptpubkey.subscribe","params":["not_hex"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.scriptpubkey.subscribe","params":["%1%",42]})" "\n";
    const auto response = get((boost::format(request) % bogus_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__bogus_script__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % bogus_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__initialization__expected_status)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // This validates the hash accumulator copy in get_scripthash_history() and incorporates
    // confirmed, rooted and unrooted transactions, duplicates, and sort.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto expected_initial = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), expected_initial);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__repeat_call__idempotent)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);
    const auto expected_initial = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_initial);

    const auto response2 = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response2.at("result").as_string(), expected_initial);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__progressive__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);

    // Confirming block 10 also makes block 11 to rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    const auto expected_confirm10 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_confirm10);

    // Confirming block 11 also makes block 12 rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    const auto expected_confirm11 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":0:"
    ));

    const auto response2 = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response2.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response2.at("result").as_string(), expected_confirm11);

    // Confirming block 12 only makes block 12 confirmed.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block12.hash()), true));
    const auto expected_confirm12 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":12:"
    ));

    const auto response3 = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response3.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response3.at("result").as_string(), expected_confirm12);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__progressive_notify__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    // This validates curosr/midstate consistency.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));
    const auto hash10 = test::bogus_block10.transactions_ptr()->at(1)->hash(false);
    const auto hash11 = test::bogus_block11.transactions_ptr()->at(0)->hash(false);
    const auto hash12 = test::bogus_block12.transactions_ptr()->at(0)->hash(false);

    // Confirming block 10 also makes block 11 to rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    const auto expected_confirm10 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":0:" +
        encode_hash(hash12) + ":-1:"
    ));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto response1 = get((boost::format(request) % found_script).str());
    REQUIRE_NO_THROW_TRUE(response1.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response1.at("result").as_string(), expected_confirm10);

    // Confirming block 11 also makes block 12 rooted.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    const auto expected_confirm11 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":0:"
    ));

    // Trigger node chaser event to electrum event subscriber.
    notify(node::chase::organized, {});

    const auto notification1 = receive();
    REQUIRE_NO_THROW_TRUE(notification1.at("method").is_string());
    REQUIRE_NO_THROW_TRUE(notification1.at("params").is_array());
    BOOST_CHECK_EQUAL(notification1.at("method").as_string(), "blockchain.scriptpubkey.subscribe");

    const auto& params1 = notification1.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params1.size(), 2u);
    BOOST_CHECK(params1.at(0).is_string());
    BOOST_CHECK(params1.at(1).is_string());
    BOOST_CHECK_EQUAL(params1.at(0).as_string(), found_scripthash);
    BOOST_CHECK_EQUAL(params1.at(1).as_string(), expected_confirm11);

    // Confirming block 12 only makes block 12 confirmed.
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block12.hash()), true));
    const auto expected_confirm12 = encode_hash(sha256_hash
    (
        encode_hash(hash10) + ":10:" +
        encode_hash(hash11) + ":11:" +
        encode_hash(hash12) + ":12:"
    ));

    // Trigger node chaser event to electrum event subscriber.
    notify(node::chase::organized, {});

    const auto notification2 = receive();
    REQUIRE_NO_THROW_TRUE(notification2.at("method").is_string());
    REQUIRE_NO_THROW_TRUE(notification2.at("params").is_array());
    BOOST_CHECK_EQUAL(notification2.at("method").as_string(), "blockchain.scriptpubkey.subscribe");

    const auto& params2 = notification2.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params2.size(), 2u);
    BOOST_CHECK(params2.at(0).is_string());
    BOOST_CHECK(params2.at(1).is_string());
    BOOST_CHECK_EQUAL(params2.at(0).as_string(), found_scripthash);
    BOOST_CHECK_EQUAL(params2.at(1).as_string(), expected_confirm12);
}

// blockchain.scriptpubkey.unsubscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_unsubscribe__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto request = R"({"id":1101,"method":"blockchain.scriptpubkey.unsubscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_unsubscribe__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":1102,"method":"blockchain.scriptpubkey.unsubscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_unsubscribe__invalid_script_encoding__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto result = get_error(R"({"id":1103,"method":"blockchain.scriptpubkey.unsubscribe","params":["not_hex"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_unsubscribe__extra_argument__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1104,"method":"blockchain.scriptpubkey.unsubscribe","params":["%1%",-1]})" "\n";
    const auto response = get((boost::format(request) % bogus_scripthash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
