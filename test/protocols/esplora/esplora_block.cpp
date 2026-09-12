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

// tip
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__tip_height__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/blocks/tip/height"), "9");
}

BOOST_AUTO_TEST_CASE(esplora__tip_hash__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/blocks/tip/hash"), encode_hash(test::block9.hash()));
}

// block-height
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__block_height__genesis__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/block-height/0"), encode_hash(test::genesis.hash()));
}

BOOST_AUTO_TEST_CASE(esplora__block_height__top__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/block-height/9"), encode_hash(test::block9.hash()));
}

BOOST_AUTO_TEST_CASE(esplora__block_height__above_top__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/block-height/10"), http::status::not_found);
}

// block
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__block__by_hash__expected)
{
    const auto hash = encode_hash(test::block1.hash());
    const auto response = get_json("/block/" + hash);
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.at("id").as_string(), hash);
    BOOST_REQUIRE_EQUAL(object.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(object.at("tx_count").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(object.at("version").as_int64(), test::block1.header().version());
    BOOST_REQUIRE_EQUAL(object.at("timestamp").as_int64(), test::block1.header().timestamp());
    BOOST_REQUIRE_EQUAL(object.at("bits").as_int64(), test::block1.header().bits());
    BOOST_REQUIRE_EQUAL(object.at("nonce").as_int64(), test::block1.header().nonce());
    BOOST_REQUIRE_EQUAL(object.at("merkle_root").as_string(), encode_hash(test::block1.header().merkle_root()));
    BOOST_REQUIRE_EQUAL(object.at("previousblockhash").as_string(), encode_hash(test::genesis.hash()));
}

BOOST_AUTO_TEST_CASE(esplora__block__genesis__no_previous)
{
    const auto response = get_json("/block/" + encode_hash(test::genesis.hash()));
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE_EQUAL(response.as_object().at("height").as_int64(), 0);
    BOOST_REQUIRE(!response.as_object().contains("previousblockhash"));
}

BOOST_AUTO_TEST_CASE(esplora__block__unknown_hash__not_found)
{
    const auto response = get_status("/block/" + encode_hash(test::mock_block10.hash()));
    BOOST_REQUIRE_EQUAL(response, http::status::not_found);
}

BOOST_AUTO_TEST_CASE(esplora__block_raw__data__matches_size)
{
    const auto hash = encode_hash(test::block1.hash());
    const auto response = get_json("/block/" + hash);
    const auto body = get_data("/block/" + hash + "/raw");
    BOOST_REQUIRE_EQUAL(body.size(), response.as_object().at("size").as_int64());
}

// block/header
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__block_header__text__expected)
{
    const auto body = get_text("/block/" + encode_hash(test::block1.hash()) + "/header");
    BOOST_REQUIRE_EQUAL(body, encode_base16(test::block1.header().to_data()));
}

// block/status
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__block_status__confirmed__next_best)
{
    const auto response = get_json("/block/" + encode_hash(test::block1.hash()) + "/status");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE(object.at("in_best_chain").as_bool());
    BOOST_REQUIRE_EQUAL(object.at("next_best").as_string(), encode_hash(test::block2.hash()));
}

BOOST_AUTO_TEST_CASE(esplora__block_status__top__no_next_best)
{
    const auto response = get_json("/block/" + encode_hash(test::block9.hash()) + "/status");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().at("in_best_chain").as_bool());
    BOOST_REQUIRE(!response.as_object().contains("next_best"));
}

// block/txids
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__block_txids__json__expected)
{
    const auto response = get_json("/block/" + encode_hash(test::block1.hash()) + "/txids");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(esplora__block_txid__index_zero__matches_txids)
{
    const auto hash = encode_hash(test::block1.hash());
    const auto response = get_json("/block/" + hash + "/txids");
    const auto body = get_text("/block/" + hash + "/txid/0");
    BOOST_REQUIRE_EQUAL(body, response.as_array().front().as_string());
}

BOOST_AUTO_TEST_CASE(esplora__block_txid__above_count__not_found)
{
    const auto status = get_status("/block/" + encode_hash(test::block1.hash()) + "/txid/1");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

// blocks
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__blocks__from_tip__ten_descending)
{
    const auto response = get_json("/blocks");
    BOOST_REQUIRE(response.is_array());

    const auto& array = response.as_array();
    BOOST_REQUIRE_EQUAL(array.size(), 10u);
    BOOST_REQUIRE_EQUAL(array.front().as_object().at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(array.back().as_object().at("height").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(esplora__blocks__from_height__truncated_at_genesis)
{
    const auto response = get_json("/blocks/5");
    BOOST_REQUIRE(response.is_array());

    const auto& array = response.as_array();
    BOOST_REQUIRE_EQUAL(array.size(), 6u);
    BOOST_REQUIRE_EQUAL(array.front().as_object().at("height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(array.back().as_object().at("height").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(esplora__blocks__above_top__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/blocks/10"), http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()
