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
    return { value.as_string().c_str() };
}

namespace {

const auto block0 = encode_hash(test::block0_hash);
const auto block5 = encode_hash(test::block5_hash);
const auto block9 = encode_hash(test::block9_hash);
const auto header9 = encode_base16(test::header9_data);

std::string block_hash_hex(const data_chunk& wire) NOEXCEPT
{
    return encode_hash(bitcoin_hash(chain::header::serialized_size(),
        wire.data()));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(bitcoind_rest_tests, bitcoind_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rest__chaininfo_json__main_nine)
{
    const auto result = rest_json("/rest/chaininfo.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblockhash")), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_json__block9_with_txs)
{
    const auto result = rest_json("/rest/block/" + block9 + ".json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE(result.at("tx").at(0).is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_hex__hashes_to_block9)
{
    const auto hex = rest_text("/rest/block/" + block9 + ".hex");
    data_chunk wire{};
    BOOST_REQUIRE(decode_base16(wire, hex));
    BOOST_REQUIRE_EQUAL(block_hash_hex(wire), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_bin__hashes_to_block9)
{
    const auto wire = rest_data("/rest/block/" + block9 + ".bin");
    BOOST_REQUIRE_EQUAL(block_hash_hex(wire), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_json__txid_list)
{
    const auto result = rest_json("/rest/block/notxdetails/" + block9 + ".json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE(result.at("tx").at(0).is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_spent_json__structured)
{
    const auto result = rest_json("/rest/block/spent/" + block9 + ".json");
    BOOST_REQUIRE(result.is_array() || result.is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__height_five__block5)
{
    const auto result = rest_json("/rest/blockhashbyheight/5.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__genesis__block0)
{
    const auto result = rest_json("/rest/blockhashbyheight/0.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")), block0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_json__count_three_from_block5)
{
    const auto result = rest_json("/rest/headers/3/" + block5 + ".json");
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 3u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_hex__one_header__eighty_bytes)
{
    const auto hex = rest_text("/rest/headers/1/" + block9 + ".hex");
    data_chunk wire{};
    BOOST_REQUIRE(decode_base16(wire, hex));
    BOOST_REQUIRE_EQUAL(wire.size(), 80u);
    BOOST_REQUIRE_EQUAL(encode_base16(wire), header9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_bin__block9_header)
{
    const auto wire = rest_data("/rest/blockpart/" + block9 + "/0/80.bin");
    BOOST_REQUIRE_EQUAL(wire.size(), 80u);
    BOOST_REQUIRE_EQUAL(encode_base16(wire), header9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilter_basic__filters_disabled__not_ok)
{
    const auto target = "/rest/blockfilter/basic/" + block9 + ".json";
    BOOST_REQUIRE(rest_status(target) != boost::beast::http::status::ok);
}

BOOST_AUTO_TEST_SUITE_END()
