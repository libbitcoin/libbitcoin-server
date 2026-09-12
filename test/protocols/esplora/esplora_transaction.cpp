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
#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "esplora_setup_fixture.hpp"

using namespace system;
using namespace boost::beast;

BOOST_FIXTURE_TEST_SUITE(esplora_tests, esplora_ten_block_setup_fixture)

// The coinbase of block one, the only transaction of that block.
static const std::string block1_hash = encode_hash(test::block1.hash());
static const std::string block1_tx = encode_hash(test::block1.transactions_ptr()->front()->hash(false));

// tx
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__tx__coinbase__expected)
{
    const auto response = get_json("/tx/" + block1_tx);
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.at("txid").as_string(), block1_tx);
    BOOST_REQUIRE_EQUAL(object.at("version").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(object.at("locktime").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(object.at("fee").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(object.at("vin").as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(object.at("vout").as_array().size(), 1u);
    BOOST_REQUIRE(object.at("vin").as_array().front().as_object().at("is_coinbase").as_bool());
}

BOOST_AUTO_TEST_CASE(esplora__tx__status__confirmed)
{
    const auto response = get_json("/tx/" + block1_tx);
    const auto& status = response.as_object().at("status").as_object();
    BOOST_REQUIRE(status.at("confirmed").as_bool());
    BOOST_REQUIRE_EQUAL(status.at("block_height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(status.at("block_hash").as_string(), block1_hash);
}

BOOST_AUTO_TEST_CASE(esplora__tx__vout__addressable)
{
    const auto response = get_json("/tx/" + block1_tx);
    const auto& output = response.as_object().at("vout").as_array().front().as_object();
    BOOST_REQUIRE_EQUAL(output.at("scriptpubkey_type").as_string(), "p2pk");
    BOOST_REQUIRE(output.at("value").as_int64() > 0);
}

BOOST_AUTO_TEST_CASE(esplora__tx__unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/tx/" + encode_hash(null_hash)), http::status::not_found);
}

BOOST_AUTO_TEST_CASE(esplora__tx_hex__text__expected)
{
    const auto body = get_text("/tx/" + block1_tx + "/hex");
    BOOST_REQUIRE_EQUAL(body, encode_base16(test::block1.transactions_ptr()->front()->to_data(true)));
}

BOOST_AUTO_TEST_CASE(esplora__tx_raw__data__expected)
{
    const auto body = get_data("/tx/" + block1_tx + "/raw");
    BOOST_REQUIRE_EQUAL(body, test::block1.transactions_ptr()->front()->to_data(true));
}

// tx/status
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__tx_status__confirmed__expected)
{
    const auto response = get_json("/tx/" + block1_tx + "/status");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().at("confirmed").as_bool());
    BOOST_REQUIRE_EQUAL(response.as_object().at("block_height").as_int64(), 1);
}

// tx/outspend(s)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__tx_outspend__unspent__expected)
{
    const auto response = get_json("/tx/" + block1_tx + "/outspend/0");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(!response.as_object().at("spent").as_bool());
}

BOOST_AUTO_TEST_CASE(esplora__tx_outspends__unspent__expected)
{
    const auto response = get_json("/tx/" + block1_tx + "/outspends");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BOOST_REQUIRE(!response.as_array().front().as_object().at("spent").as_bool());
}

// tx/merkle-proof
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__tx_merkle_proof__single__empty_branch)
{
    const auto response = get_json("/tx/" + block1_tx + "/merkle-proof");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.at("block_height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(object.at("pos").as_int64(), 0);
    BOOST_REQUIRE(object.at("merkle").as_array().empty());
}

// block/txs
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__block_txs__default__expected)
{
    const auto response = get_json("/block/" + block1_hash + "/txs");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(response.as_array().front().as_object().at("txid").as_string(), block1_tx);
}

BOOST_AUTO_TEST_CASE(esplora__block_txs__unaligned_start__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/block/" + block1_hash + "/txs/3"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__block_txs__start_above_count__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/block/" + block1_hash + "/txs/25"), http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()
