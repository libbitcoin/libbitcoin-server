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
#include "../blocks.hpp"
#include "electrum.hpp"
#include <boost/format.hpp>

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

// blockchain.transaction.broadcast

using namespace system;
static const code not_found{ server::error::not_found };
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };
static const code unconfirmable_transaction{ server::error::unconfirmable_transaction };

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__empty__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":74,"method":"blockchain.transaction.broadcast","params":[""]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__invalid_encoding__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":75,"method":"blockchain.transaction.broadcast","params":["deadbeef"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__invalid_tx__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast","params":["0100000001"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__genesis_coinbase__unconfirmable_transaction)
{
    BOOST_CHECK(handshake());

    const auto tx0_text = encode_base16(genesis.transactions_ptr()->front()->to_data(true));
    constexpr auto request = R"({"id":73,"method":"blockchain.transaction.broadcast","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx0_text).str());
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), unconfirmable_transaction.value());
}

// blockchain.transaction.get

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__empty_hash__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":77,"method":"blockchain.transaction.get","params":["",false]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__invalid_hash__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":78,"method":"blockchain.transaction.get","params":["deadbeef",false]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__nonexistent_tx__not_found)
{
    BOOST_CHECK(handshake());

    const auto bogus = "0000000000000000000000000000000000000000000000000000000000000042";
    const auto request = R"({"id":79,"method":"blockchain.transaction.get","params":["%1%",false]})" "\n";
    const auto response = get((boost::format(request) % bogus).str());
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__missing_param__dropped)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":80,"method":"blockchain.transaction.get","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    BOOST_CHECK(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__extra_param__dropped)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":81,"method":"blockchain.transaction.get","params":["%1%",false,"extra"]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    BOOST_CHECK(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__genesis_coinbase_verbose_false__expected)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":82,"method":"blockchain.transaction.get","params":["%1%",false]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    BOOST_CHECK_EQUAL(response.at("result").as_string(), encode_base16(coinbase.to_data(true)));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__genesis_coinbase_verbose_true__expected)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":83,"method":"blockchain.transaction.get","params":["%1%",true]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());

    auto expected = value_from(bitcoind(coinbase));
    BOOST_CHECK(expected.is_object());

    // The test store is ten confirmed blocks, so top height of nine.
    constexpr auto top = 9u;
    auto& tx = expected.as_object();
    tx["in_active_chain"] = true;
    tx["blockhash"] = encode_hash(genesis.hash());
    tx["confirmations"] = add1(top);
    tx["blocktime"] = genesis.header().timestamp();
    tx["time"] = genesis.header().timestamp();
    BOOST_CHECK_EQUAL(response.at("result").as_object(), expected);
}

// blockchain.transaction.get_merkle

// blockchain.transaction.id_from_pos

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__genesis_coinbase_default__expected)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":90,"method":"blockchain.transaction.id_from_pos","params":[0,0]})" "\n";
    const auto response = get(request);
    BOOST_CHECK_EQUAL(response.at("result").as_string(), tx0_hash);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__coinbase_false__expected)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *block2.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":91,"method":"blockchain.transaction.id_from_pos","params":[2,0,false]})" "\n";
    const auto response = get(request);
    BOOST_CHECK_EQUAL(response.at("result").as_string(), tx0_hash);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__merkle_proof_one_tx__empty)
{
    BOOST_CHECK(handshake());

    const auto& coinbase = *block9.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":92,"method":"blockchain.transaction.id_from_pos","params":[9,0,true]})" "\n";
    const auto& object = get(request).at("result").as_object();
    BOOST_CHECK_EQUAL(object.at("tx_hash").as_string(), tx0_hash);
    BOOST_CHECK(object.at("merkle").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__missing_block__not_found)
{
    BOOST_CHECK(handshake());

    const auto request = R"({"id":93,"method":"blockchain.transaction.id_from_pos","params":[11,0]})" "\n";
    BOOST_CHECK_EQUAL(get(request).at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__missing_position__not_found)
{
    BOOST_CHECK(handshake());

    const auto request = R"({"id":94,"method":"blockchain.transaction.id_from_pos","params":[0,1]})" "\n";
    BOOST_CHECK_EQUAL(get(request).at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_SUITE_END()
