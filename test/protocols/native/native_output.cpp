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
#include "native_setup_fixture.hpp"

using namespace system;
using namespace boost::beast;

static const auto& tx1 = *test::block1a.transactions_ptr()->front();
static const auto& tx1_outputs = *tx1.outputs_ptr();
static const std::string tx1_hash = encode_hash(tx1.hash(false));
static const auto& tx2 = *test::block2a.transactions_ptr()->front();
static const std::string tx2_hash = encode_hash(tx2.hash(false));

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(native__outputs__unknown__not_found)
{
    const auto hash = encode_hash(null_hash);
    BOOST_REQUIRE_EQUAL(get_status("/v1/output/" + hash + "?format=data"), http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(native_witness_tests, native_witness_setup_fixture)

// outputs
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__outputs__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/output/" + tx1_hash + "?format=data"), base16_chunk("18000000000000000179" "2a00000000000000017a"));
}

BOOST_AUTO_TEST_CASE(native__outputs__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/output/" + tx1_hash + "?format=text"), "18000000000000000179" "2a00000000000000017a");
}

BOOST_AUTO_TEST_CASE(native__outputs__json__expected)
{
    const auto response = get_json("/v1/output/" + tx1_hash + "?format=json");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 2u);
    BOOST_REQUIRE_EQUAL(response.as_array().at(0).as_object().at("value").as_int64(), 0x18);
    BOOST_REQUIRE_EQUAL(response.as_array().at(1).as_object().at("value").as_int64(), 0x2a);
    BOOST_REQUIRE_EQUAL(response.as_array().at(1), value_from(*tx1_outputs.at(1)));
}

// output
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/output/" + tx1_hash + "/0?format=data"), base16_chunk("18000000000000000179"));
}

BOOST_AUTO_TEST_CASE(native__output__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/output/" + tx1_hash + "/1?format=text"), "2a00000000000000017a");
}

BOOST_AUTO_TEST_CASE(native__output__json__expected)
{
    const auto response = get_json("/v1/output/" + tx1_hash + "/0?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE_EQUAL(response.as_object().at("value").as_int64(), 0x18);
    BOOST_REQUIRE_EQUAL(response, value_from(*tx1_outputs.at(0)));
}

BOOST_AUTO_TEST_CASE(native__output__index_above_count__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/output/" + tx1_hash + "/2?format=data"), http::status::not_found);
}

// output/script
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_script__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/output/" + tx1_hash + "/0/script?format=data"), base16_chunk("79"));
}

BOOST_AUTO_TEST_CASE(native__output_script__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/output/" + tx1_hash + "/1/script?format=text"), "7a");
}

BOOST_AUTO_TEST_CASE(native__output_script__json__expected)
{
    const auto& script = tx1_outputs.at(1)->script();
    BOOST_REQUIRE_EQUAL(get_json("/v1/output/" + tx1_hash + "/1/script?format=json"), value_from(script));
}

BOOST_AUTO_TEST_CASE(native__output_script__index_above_count__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/output/" + tx1_hash + "/2/script?format=data"), http::status::not_found);
}

// output/spender
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_spender__data__expected)
{
    const auto expected = splice(tx2.hash(false), base16_chunk("01000000"));
    BOOST_REQUIRE_EQUAL(get_data("/v1/output/" + tx1_hash + "/1/spender?format=data"), expected);
}

BOOST_AUTO_TEST_CASE(native__output_spender__text__expected)
{
    const auto expected = encode_base16(tx2.hash(false)) + "00000000";
    BOOST_REQUIRE_EQUAL(get_text("/v1/output/" + tx1_hash + "/0/spender?format=text"), expected);
}

BOOST_AUTO_TEST_CASE(native__output_spender__json__expected)
{
    const auto response = get_json("/v1/output/" + tx1_hash + "/0/spender?format=json");
    BOOST_REQUIRE_EQUAL(response, value_from(chain::point{ tx2.hash(false), 0 }));
}

BOOST_AUTO_TEST_CASE(native__output_spender__unspent__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/output/" + tx2_hash + "/0/spender?format=data"), http::status::not_found);
}

// output/spenders
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_spenders__data__expected)
{
    const auto expected = splice(tx2.hash(false), base16_chunk("00000000"));
    BOOST_REQUIRE_EQUAL(get_data("/v1/output/" + tx1_hash + "/0/spenders?format=data"), expected);
}

BOOST_AUTO_TEST_CASE(native__output_spenders__text__expected)
{
    const auto expected = encode_base16(tx2.hash(false)) + "01000000";
    BOOST_REQUIRE_EQUAL(get_text("/v1/output/" + tx1_hash + "/1/spenders?format=text"), expected);
}

BOOST_AUTO_TEST_CASE(native__output_spenders__json__expected)
{
    const auto response = get_json("/v1/output/" + tx1_hash + "/1/spenders?format=json");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(native__output_spenders__unspent__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/output/" + tx2_hash + "/0/spenders?format=data"), http::status::not_found);
}

// output/subscribe
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_subscribe__data__output)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/output/" + tx1_hash + "/0/subscribe?format=data"), base16_chunk("18000000000000000179"));
}

BOOST_AUTO_TEST_CASE(native__ws_output_subscribe__stop__empty)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_get_text("/v1/output/" + tx1_hash + "/0/subscribe?stop=true").empty());
}

BOOST_AUTO_TEST_SUITE_END()
