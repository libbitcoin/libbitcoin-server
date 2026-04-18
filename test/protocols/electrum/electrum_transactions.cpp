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

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// blockchain.transaction.broadcast

using namespace system;
static const code not_found{ server::error::not_found };
static const code wrong_version{ server::error::wrong_version };
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };
static const code unsupported_argument{ server::error::unsupported_argument };
static const code unconfirmable_transaction{ server::error::unconfirmable_transaction };
static const code coinbase_transaction{ system::error::coinbase_transaction };

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__empty__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":74,"method":"blockchain.transaction.broadcast","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__invalid_encoding__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":75,"method":"blockchain.transaction.broadcast","params":["xxxx"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__invalid_tx__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast","params":["0100000001"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__v1_0_genesis_coinbase__coinbase_transaction_response)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto tx0_text = encode_base16(test::genesis.transactions_ptr()->front()->to_data(true));
    constexpr auto request = R"({"id":73,"method":"blockchain.transaction.broadcast","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx0_text).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), coinbase_transaction.message());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__v1_6_genesis_coinbase__coinbase_transaction_error)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto tx0_text = encode_base16(test::genesis.transactions_ptr()->front()->to_data(true));
    constexpr auto request = R"({"id":74,"method":"blockchain.transaction.broadcast","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx0_text).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), coinbase_transaction.value());
}

// blockchain.transaction.broadcast_package

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__empty_v1_4__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast_package","params":[[]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__empty_verbose__unsupported_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast_package","params":[[],true]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), unsupported_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__string__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast_package","params":[""]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__empty__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast_package","params":[[]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__not_string__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":75,"method":"blockchain.transaction.broadcast_package","params":[[true,false]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__invalid_encoding__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":75,"method":"blockchain.transaction.broadcast_package","params":[["xxxx"]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__invalid_tx__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast_package","params":[["0100000001"]]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast_package__two_transactions__unconfirmable_transaction)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto tx0_text = encode_base16(test::genesis.transactions_ptr()->front()->to_data(true));
    const auto tx1_text = encode_base16(test::block1.transactions_ptr()->front()->to_data(true));
    const auto tx0_hash = encode_hash(test::genesis.transactions_ptr()->front()->hash(false));
    const auto tx1_hash = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    constexpr auto request = R"({"id":73,"method":"blockchain.transaction.broadcast_package","params":[["%1%","%2%"]]})" "\n";
    const auto response = get((boost::format(request) % tx0_text % tx1_text).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("success").is_bool());
    REQUIRE_NO_THROW_TRUE(result.at("errors").is_array());
    BOOST_REQUIRE(!result.at("success").as_bool());

    const auto& errors = result.at("errors").as_array();
    BOOST_REQUIRE_EQUAL(errors.size(), 2u);
    BOOST_REQUIRE(errors.at(0).is_object());
    BOOST_REQUIRE(errors.at(1).is_object());

    const auto& error1 = errors.at(0).as_object();
    REQUIRE_NO_THROW_TRUE(error1.at("txid").is_string());
    REQUIRE_NO_THROW_TRUE(error1.at("error").is_string());
    BOOST_REQUIRE_EQUAL(error1.at("txid").as_string(), tx0_hash);
    BOOST_REQUIRE_EQUAL(error1.at("error").as_string(), coinbase_transaction.message());

    const auto& error2 = errors.at(1).as_object();
    REQUIRE_NO_THROW_TRUE(error2.at("txid").is_string());
    REQUIRE_NO_THROW_TRUE(error2.at("error").is_string());
    BOOST_REQUIRE_EQUAL(error2.at("txid").as_string(), tx1_hash);
    BOOST_REQUIRE_EQUAL(error2.at("error").as_string(), coinbase_transaction.message());
}

// blockchain.transaction.get

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__empty_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":77,"method":"blockchain.transaction.get","params":["",false]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":78,"method":"blockchain.transaction.get","params":["deadbeef",false]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__nonexistent_tx__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto bogus = "0000000000000000000000000000000000000000000000000000000000000042";
    const auto request = R"({"id":79,"method":"blockchain.transaction.get","params":["%1%",false]})" "\n";
    const auto response = get((boost::format(request) % bogus).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__missing_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":80,"method":"blockchain.transaction.get","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":81,"method":"blockchain.transaction.get","params":["%1%",false,"extra"]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__genesis_coinbase_verbose_false__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":82,"method":"blockchain.transaction.get","params":["%1%",false]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), encode_base16(coinbase.to_data(true)));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__version_1_1_verbose__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":83,"method":"blockchain.transaction.get","params":["%1%",true]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get__version_1_2_verbose__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":83,"method":"blockchain.transaction.get","params":["%1%",true]})" "\n";
    const auto response = get((boost::format(request) % tx0_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    auto expected = value_from(bitcoind(coinbase));
    BOOST_REQUIRE(expected.is_object());

    // The test store is ten confirmed blocks, so top height of nine.
    constexpr auto top = 9u;
    auto& tx = expected.as_object();
    tx["in_active_chain"] = true;
    tx["blockhash"] = encode_hash(test::genesis.hash());
    tx["confirmations"] = add1(top);
    tx["blocktime"] = test::genesis.header().timestamp();
    tx["time"] = test::genesis.header().timestamp();
    BOOST_REQUIRE_EQUAL(response.at("result").as_object(), expected);
}

// blockchain.transaction.get_merkle

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto response = get(R"({"id":100,"method":"blockchain.transaction.get_merkle","params":["",0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__empty_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":100,"method":"blockchain.transaction.get_merkle","params":["",0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__invalid_hash__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":101,"method":"blockchain.transaction.get_merkle","params":["deadbeef",0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__nonexistent_height__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto bogus = "0000000000000000000000000000000000000000000000000000000000000042";
    const auto request = R"({"id":102,"method":"blockchain.transaction.get_merkle","params":["%1%",999]})" "\n";
    const auto response = get((boost::format(request) % bogus).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__tx_not_in_block__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto bogus = "0000000000000000000000000000000000000000000000000000000000000042";
    const auto request = R"({"id":103,"method":"blockchain.transaction.get_merkle","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % bogus).str());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__genesis_coinbase__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":104,"method":"blockchain.transaction.get_merkle","params":["%1%",0]})" "\n";
    const auto response = get((boost::format(request) % tx_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("block_height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("pos").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("merkle").is_array());
    BOOST_REQUIRE_EQUAL(result.at("block_height").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("pos").as_int64(), 0);
    BOOST_REQUIRE(result.at("merkle").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__mutiple_txs_block__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& txs = *test::bogus_block10.transactions_ptr();
    const auto& tx0 = *txs.at(0);
    const auto& tx1 = *txs.at(1);
    const auto& tx2 = *txs.at(2);
    const auto tx0_hash = tx0.hash(false);
    const auto tx1_hash = tx1.hash(false);
    const auto tx2_hash = tx2.hash(false);

    // Add a confirmed multi-tx block.
    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));

    const auto request = R"({"id":104,"method":"blockchain.transaction.get_merkle","params":["%1%",10]})" "\n";
    const auto response = get((boost::format(request) % encode_hash(tx1_hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("block_height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("pos").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("merkle").is_array());
    BOOST_REQUIRE_EQUAL(result.at("block_height").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(result.at("pos").as_int64(), 1);

    const auto& merkle = result.at("merkle").as_array();
    BOOST_REQUIRE_EQUAL(merkle.size(), 2u);
    BOOST_REQUIRE(merkle.at(0).is_string());
    BOOST_REQUIRE_EQUAL(merkle.at(0).as_string(), encode_hash(tx0_hash));
    BOOST_REQUIRE_EQUAL(merkle.at(1).as_string(), encode_hash(bitcoin_hash(tx2_hash, tx2_hash)));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__missing_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":105,"method":"blockchain.transaction.get_merkle","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_get_merkle__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx_hash = encode_hash(coinbase.hash(false));
    const auto request = R"({"id":106,"method":"blockchain.transaction.get_merkle","params":["%1%",0,"extra"]})" "\n";
    const auto response = get((boost::format(request) % tx_hash).str());
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

// blockchain.transaction.id_from_pos

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto response = get(R"({"id":90,"method":"blockchain.transaction.id_from_pos","params":[0,0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__genesis_coinbase_default__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& coinbase = *test::genesis.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto response = get(R"({"id":90,"method":"blockchain.transaction.id_from_pos","params":[0,0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), tx0_hash);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__coinbase_false__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& coinbase = *test::block2.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto response = get(R"({"id":91,"method":"blockchain.transaction.id_from_pos","params":[2,0,false]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), tx0_hash);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__merkle_proof_one_tx__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto& coinbase = *test::block9.transactions_ptr()->front();
    const auto tx0_hash = encode_hash(coinbase.hash(false));
    const auto response = get(R"({"id":92,"method":"blockchain.transaction.id_from_pos","params":[9,0,true]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& object = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(object.at("tx_hash").is_string());
    REQUIRE_NO_THROW_TRUE(object.at("merkle").is_array());
    BOOST_REQUIRE_EQUAL(object.at("tx_hash").as_string(), tx0_hash);
    BOOST_REQUIRE(object.at("merkle").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__missing_block__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":93,"method":"blockchain.transaction.id_from_pos","params":[11,0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_id_from_pos__missing_position__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":94,"method":"blockchain.transaction.id_from_pos","params":[0,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_SUITE_END()
