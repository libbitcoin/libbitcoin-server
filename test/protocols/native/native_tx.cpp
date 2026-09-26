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

static const std::string coinbase1_hash = "0e3e2357e806b6cdb1f70b54c3a3a17b6714ee1f0e68bebb44a74b1efd512098";
static const auto& coinbase1 = *test::block1.transactions_ptr()->front();

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// tx
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/tx/" + coinbase1_hash + "?format=data"), coinbase1.to_data(true));
}

BOOST_AUTO_TEST_CASE(native__tx__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/tx/" + coinbase1_hash + "?format=text"), encode_base16(coinbase1.to_data(true)));
}

BOOST_AUTO_TEST_CASE(native__tx__json__expected)
{
    const auto response = get_json("/v1/tx/" + coinbase1_hash + "?format=json");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.at("hash").as_string(), coinbase1_hash);
    BOOST_REQUIRE_EQUAL(object.at("version").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(object.at("inputs").as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(object.at("outputs").as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(native__tx__unknown__not_found)
{
    const auto hash = encode_hash(null_hash);
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + hash + "?format=data"), http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__tx__invalid_hash__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/42?format=data"), http::status::bad_request);
}

// tx/header
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx_header__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/tx/" + coinbase1_hash + "/header?format=data"), to_chunk(test::header1_data));
}

BOOST_AUTO_TEST_CASE(native__tx_header__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/tx/" + coinbase1_hash + "/header?format=text"), encode_base16(test::header1_data));
}

BOOST_AUTO_TEST_CASE(native__tx_header__json__expected)
{
    const auto response = get_json("/v1/tx/" + coinbase1_hash + "/header?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE_EQUAL(response.as_object().at("hash").as_string(), encode_hash(test::block1_hash));
    BOOST_REQUIRE_EQUAL(response.as_object().at("height").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(native__tx_header__unknown__not_found)
{
    const auto hash = encode_hash(null_hash);
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + hash + "/header?format=data"), http::status::not_found);
}

// tx/details
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx_details__coinbase__expected)
{
    const auto response = get_json("/v1/tx/" + coinbase1_hash + "/details?format=json");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE(!object.at("segregated").as_bool());
    BOOST_REQUIRE(object.at("coinbase").as_bool());
    BOOST_REQUIRE_EQUAL(object.at("nominal").as_int64(), 134);
    BOOST_REQUIRE_EQUAL(object.at("maximal").as_int64(), 134);
    BOOST_REQUIRE_EQUAL(object.at("weight").as_int64(), 536);
    BOOST_REQUIRE_EQUAL(object.at("virtual").as_int64(), 134);
    BOOST_REQUIRE_EQUAL(object.at("value").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(object.at("spend").as_int64(), 5000000000);
    BOOST_REQUIRE_EQUAL(object.at("fee").as_int64(), 0);

    const auto& confirmed = object.at("confirmed").as_object();
    BOOST_REQUIRE_EQUAL(confirmed.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(confirmed.at("position").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(native__tx_details__text__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + coinbase1_hash + "/details?format=text"), http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__tx_details__unknown__not_found)
{
    const auto hash = encode_hash(null_hash);
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + hash + "/details?format=json"), http::status::not_found);
}

// tx/subscribe
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__ws_tx_subscribe__notify_text__expected)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_get_text("/v1/tx/subscribe?format=text").empty());

    const auto link = query_.to_tx(coinbase1.hash(false));
    notify(node::chases::transaction{ link.value });

    BOOST_REQUIRE_EQUAL(to_string(ws_receive()), encode_base16(coinbase1.hash(false)));
}

BOOST_AUTO_TEST_CASE(native__ws_tx_subscribe__notify_data__expected)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_get_text("/v1/tx/subscribe?format=data").empty());

    const auto link = query_.to_tx(coinbase1.hash(false));
    notify(node::chases::transaction{ link.value });

    BOOST_REQUIRE_EQUAL(ws_receive(), to_chunk(coinbase1.hash(false)));
}

BOOST_AUTO_TEST_CASE(native__ws_tx_subscribe__stop__empty)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_get_text("/v1/tx/subscribe?stop=true").empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(native_address_tests, native_address_setup_fixture)

// tx (unconfirmed witness)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx__witness__expected)
{
    const auto hash = encode_hash(test::tx4.hash(false));
    BOOST_REQUIRE_EQUAL(get_data("/v1/tx/" + hash + "?format=data"), test::tx4.to_data(true));
}

BOOST_AUTO_TEST_CASE(native__tx__no_witness__expected)
{
    const auto hash = encode_hash(test::tx4.hash(false));
    BOOST_REQUIRE_EQUAL(get_data("/v1/tx/" + hash + "?format=data&witness=false"), test::tx4.to_data(false));
}

BOOST_AUTO_TEST_CASE(native__tx_header__unconfirmed__not_found)
{
    const auto hash = encode_hash(test::tx4.hash(false));
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + hash + "/header?format=data"), http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__tx_details__unconfirmed_witness__expected)
{
    const auto hash = encode_hash(test::tx4.hash(false));
    const auto response = get_json("/v1/tx/" + hash + "/details?format=json");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE(object.at("segregated").as_bool());
    BOOST_REQUIRE(!object.at("coinbase").as_bool());
    BOOST_REQUIRE_EQUAL(object.at("nominal").as_int64(), test::tx4.serialized_size(false));
    BOOST_REQUIRE_EQUAL(object.at("maximal").as_int64(), test::tx4.serialized_size(true));
    BOOST_REQUIRE_EQUAL(object.at("weight").as_int64(), test::tx4.weight());
    BOOST_REQUIRE_EQUAL(object.at("virtual").as_int64(), test::tx4.virtual_size());
    BOOST_REQUIRE_EQUAL(object.at("value").as_int64(), 0x18 + 0x2a);
    BOOST_REQUIRE_EQUAL(object.at("spend").as_int64(), 0x08);
    BOOST_REQUIRE_EQUAL(object.at("fee").as_int64(), 0x18 + 0x2a - 0x08);
    BOOST_REQUIRE(!object.contains("confirmed"));
}

BOOST_AUTO_TEST_CASE(native__tx_details__spend_exceeds_value__internal_server_error)
{
    const auto& tx2 = *test::block2a.transactions_ptr()->front();
    const auto hash = encode_hash(tx2.hash(false));
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + hash + "/details?format=json"), http::status::internal_server_error);
}

BOOST_AUTO_TEST_SUITE_END()
