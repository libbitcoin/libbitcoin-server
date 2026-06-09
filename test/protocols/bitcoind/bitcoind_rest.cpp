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
#include "bitcoind_setup_fixture.hpp"

using namespace system;

static std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return std::string{ value.as_string().c_str() };
}

// Reconstruct a block from wire bytes and return its hash as display hex.
static std::string block_hash_hex(const data_chunk& wire) NOEXCEPT
{
    return encode_hash(chain::block{ wire, true }.hash());
}

// The ten-block store contains mainnet blocks 0..9 (block9 is the tip).
BOOST_FIXTURE_TEST_SUITE(bitcoind_rest_tests, bitcoind_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rest__chaininfo_json__main_nine)
{
    const auto result = rest_json("/rest/chaininfo.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblockhash")),
        encode_hash(test::block9_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_json__block9_with_txs)
{
    const auto target = "/rest/block/" + encode_hash(test::block9_hash) + ".json";
    const auto result = rest_json(target);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")),
        encode_hash(test::block9_hash));
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE(result.at("tx").at(0).is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_hex__hashes_to_block9)
{
    auto hex = rest_text("/rest/block/" + encode_hash(test::block9_hash) + ".hex");
    while (!hex.empty() && (hex.back() == '\n' || hex.back() == '\r'))
        hex.pop_back();

    data_chunk wire{};
    BOOST_REQUIRE(decode_base16(wire, hex));
    BOOST_REQUIRE_EQUAL(block_hash_hex(wire), encode_hash(test::block9_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_bin__hashes_to_block9)
{
    const auto wire = rest_data("/rest/block/" + encode_hash(test::block9_hash) + ".bin");
    BOOST_REQUIRE_EQUAL(block_hash_hex(wire), encode_hash(test::block9_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_json__txid_list)
{
    const auto target = "/rest/block/notxdetails/" +
        encode_hash(test::block9_hash) + ".json";
    const auto result = rest_json(target);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")),
        encode_hash(test::block9_hash));
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE(result.at("tx").at(0).is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_spent_json__structured)
{
    const auto target = "/rest/block/spent/" +
        encode_hash(test::block9_hash) + ".json";
    const auto result = rest_json(target);
    BOOST_REQUIRE(result.is_array() || result.is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__height_five__block5)
{
    const auto result = rest_json("/rest/blockhashbyheight/5.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")),
        encode_hash(test::block5_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__genesis__block0)
{
    const auto result = rest_json("/rest/blockhashbyheight/0.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")),
        encode_hash(test::block0_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_json__count_three_from_block5)
{
    const auto target = "/rest/headers/3/" + encode_hash(test::block5_hash) + ".json";
    const auto result = rest_json(target);
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 3u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0).at("hash")),
        encode_hash(test::block5_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_hex__one_header__eighty_bytes)
{
    auto hex = rest_text("/rest/headers/1/" + encode_hash(test::block9_hash) + ".hex");
    while (!hex.empty() && (hex.back() == '\n' || hex.back() == '\r'))
        hex.pop_back();

    data_chunk wire{};
    BOOST_REQUIRE(decode_base16(wire, hex));
    BOOST_REQUIRE_EQUAL(wire.size(), 80u);
    BOOST_REQUIRE_EQUAL(encode_base16(wire), encode_base16(test::header9_data));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_bin__block9_header)
{
    const auto target = "/rest/blockpart/" + encode_hash(test::block9_hash) +
        "/0/80.bin";
    const auto wire = rest_data(target);
    BOOST_REQUIRE_EQUAL(wire.size(), 80u);
    BOOST_REQUIRE_EQUAL(encode_base16(wire), encode_base16(test::header9_data));
}

// blockfilter requires bip158 (disabled in this store) -> non-200.
BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilter_basic__filters_disabled__not_ok)
{
    const auto target = "/rest/blockfilter/basic/" +
        encode_hash(test::block9_hash) + ".json";
    BOOST_REQUIRE(rest_status(target) != boost::beast::http::status::ok);
}

BOOST_AUTO_TEST_SUITE_END()
