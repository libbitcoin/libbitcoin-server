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
static const auto& tx1_inputs = *tx1.inputs_ptr();
static const std::string tx1_hash = encode_hash(tx1.hash(false));

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(native__inputs__coinbase__expected)
{
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto hash = encode_hash(coinbase.hash(false));
    BOOST_REQUIRE_EQUAL(get_data("/v1/input/" + hash + "?format=data"), coinbase.inputs_ptr()->front()->to_data());
}

BOOST_AUTO_TEST_CASE(native__inputs__unknown__not_found)
{
    const auto hash = encode_hash(null_hash);
    BOOST_REQUIRE_EQUAL(get_status("/v1/input/" + hash + "?format=data"), http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(native_witness_tests, native_witness_setup_fixture)

// inputs
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__inputs__data__expected)
{
    const auto input0 = tx1_inputs.at(0)->to_data();
    const auto input1 = tx1_inputs.at(1)->to_data();
    const auto input2 = tx1_inputs.at(2)->to_data();
    const auto expected = build_chunk({ input0, input1, input2 });
    BOOST_REQUIRE_EQUAL(get_data("/v1/input/" + tx1_hash + "?format=data"), expected);
}

BOOST_AUTO_TEST_CASE(native__inputs__text__expected)
{
    const auto input0 = tx1_inputs.at(0)->to_data();
    const auto input1 = tx1_inputs.at(1)->to_data();
    const auto input2 = tx1_inputs.at(2)->to_data();
    const auto expected = build_chunk({ input0, input1, input2 });
    BOOST_REQUIRE_EQUAL(get_text("/v1/input/" + tx1_hash + "?format=text"), encode_base16(expected));
}

BOOST_AUTO_TEST_CASE(native__inputs__json__expected)
{
    const auto response = get_json("/v1/input/" + tx1_hash + "?format=json");
    BOOST_REQUIRE(response.is_array());

    const auto& inputs = response.as_array();
    BOOST_REQUIRE_EQUAL(inputs.size(), 3u);
    BOOST_REQUIRE_EQUAL(inputs.at(0).as_object().at("sequence").as_int64(), 0x2a);
    BOOST_REQUIRE_EQUAL(inputs.at(1).as_object().at("sequence").as_int64(), 0x18);
    BOOST_REQUIRE_EQUAL(inputs.at(2).as_object().at("sequence").as_int64(), 0x19);
    BOOST_REQUIRE_EQUAL(inputs.at(0), value_from(*tx1_inputs.at(0)));
    BOOST_REQUIRE_EQUAL(inputs.at(2), value_from(*tx1_inputs.at(2)));
}

// input
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__input__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/input/" + tx1_hash + "/1?format=data"), tx1_inputs.at(1)->to_data());
}

BOOST_AUTO_TEST_CASE(native__input__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/input/" + tx1_hash + "/2?format=text"), encode_base16(tx1_inputs.at(2)->to_data()));
}

BOOST_AUTO_TEST_CASE(native__input__json__expected)
{
    const auto response = get_json("/v1/input/" + tx1_hash + "/1?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE_EQUAL(response.as_object().at("sequence").as_int64(), 0x18);
    BOOST_REQUIRE(response.as_object().contains("witness"));
    BOOST_REQUIRE_EQUAL(response, value_from(*tx1_inputs.at(1)));
}

BOOST_AUTO_TEST_CASE(native__input__index_above_count__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/input/" + tx1_hash + "/3?format=data"), http::status::not_found);
}

// input/script
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__input_script__data__expected)
{
    const auto& script = tx1_inputs.at(0)->script();
    BOOST_REQUIRE_EQUAL(get_data("/v1/input/" + tx1_hash + "/0/script?format=data"), script.to_data(false));
}

BOOST_AUTO_TEST_CASE(native__input_script__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/input/" + tx1_hash + "/1/script?format=text"), "6a7a");
}

BOOST_AUTO_TEST_CASE(native__input_script__json__expected)
{
    const auto& script = tx1_inputs.at(2)->script();
    BOOST_REQUIRE_EQUAL(get_json("/v1/input/" + tx1_hash + "/2/script?format=json"), value_from(script));
}

BOOST_AUTO_TEST_CASE(native__input_script__index_above_count__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/input/" + tx1_hash + "/3/script?format=data"), http::status::not_found);
}

// input/witness
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__input_witness__data__expected)
{
    const auto& witness = tx1_inputs.at(0)->witness();
    BOOST_REQUIRE_EQUAL(get_data("/v1/input/" + tx1_hash + "/0/witness?format=data"), witness.to_data(false));
}

BOOST_AUTO_TEST_CASE(native__input_witness__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/input/" + tx1_hash + "/1/witness?format=text"), "03313131");
}

BOOST_AUTO_TEST_CASE(native__input_witness__json__expected)
{
    const auto& witness = tx1_inputs.at(2)->witness();
    BOOST_REQUIRE_EQUAL(get_json("/v1/input/" + tx1_hash + "/2/witness?format=json"), value_from(witness));
}

BOOST_AUTO_TEST_CASE(native__input_witness__index_above_count__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/input/" + tx1_hash + "/3/witness?format=data"), http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()
