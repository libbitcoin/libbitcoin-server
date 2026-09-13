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

static const auto& coinbase1 = *test::block1.transactions_ptr()->front();
static const std::string block1_tx = encode_hash(coinbase1.hash(false));
static const std::string scripthash = encode_hash(null_hash);
static const std::string funded_scripthash = encode_hash(sha256_hash(coinbase1.outputs_ptr()->front()->script().to_data(false)));

// key resolution
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__address__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/address/notanaddress"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__address_txs__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/address/notanaddress/txs"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__address_utxo__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/address/notanaddress/utxo"), http::status::bad_request);
}

// address/txs/mempool
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__address_txs_mempool__scripthash__empty)
{
    const auto response = get_json("/scripthash/" + scripthash + "/txs/mempool");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE(response.as_array().empty());
}

// scripthash of the block one coinbase output
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__address__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash);
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.at("scripthash").as_string(), funded_scripthash);

    const auto& stats = object.at("chain_stats").as_object();
    BOOST_REQUIRE_EQUAL(stats.at("funded_txo_count").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(stats.at("funded_txo_sum").as_int64(), 5000000000);
    BOOST_REQUIRE_EQUAL(stats.at("spent_txo_count").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(stats.at("spent_txo_sum").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(stats.at("tx_count").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(esplora__address__unfunded__zeroed)
{
    const auto response = get_json("/scripthash/" + scripthash);
    BOOST_REQUIRE(response.is_object());

    const auto& stats = response.as_object().at("chain_stats").as_object();
    BOOST_REQUIRE_EQUAL(stats.at("funded_txo_count").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(stats.at("tx_count").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(esplora__address_txs__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash + "/txs");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(response.as_array().front().as_object().at("txid").as_string(), block1_tx);
}

BOOST_AUTO_TEST_CASE(esplora__address_utxo__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash + "/utxo");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);

    const auto& utxo = response.as_array().front().as_object();
    BOOST_REQUIRE_EQUAL(utxo.at("txid").as_string(), block1_tx);
    BOOST_REQUIRE_EQUAL(utxo.at("vout").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(utxo.at("value").as_int64(), 5000000000);
    BOOST_REQUIRE(utxo.at("status").as_object().at("confirmed").as_bool());
}

BOOST_AUTO_TEST_CASE(esplora__address_utxo__unfunded__empty)
{
    const auto response = get_json("/scripthash/" + scripthash + "/utxo");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE(response.as_array().empty());
}

BOOST_AUTO_TEST_SUITE_END()
