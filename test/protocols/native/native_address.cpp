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

static const auto& coinbase1 = *test::block1.transactions_ptr()->front();
static const auto& output1 = *coinbase1.outputs_ptr()->front();
static const std::string funded = encode_hash(sha256_hash(output1.script().to_data(false)));
static const std::string unfunded = encode_hash(null_hash);
static const chain::outpoint outpoint1{ chain::point{ coinbase1.hash(false), 0 }, 5000000000 };

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// address
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__address__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/address/" + funded + "?format=data"), outpoint1.to_data());
}

BOOST_AUTO_TEST_CASE(native__address__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/address/" + funded + "?format=text"), encode_base16(outpoint1.to_data()));
}

BOOST_AUTO_TEST_CASE(native__address__json__expected)
{
    const auto response = get_json("/v1/address/" + funded + "?format=json");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(response.as_array().front().as_object().at("value").as_int64(), 5000000000);
    BOOST_REQUIRE_EQUAL(response.as_array().front(), value_from(outpoint1));
}

BOOST_AUTO_TEST_CASE(native__address__no_turbo__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/address/" + funded + "?format=data&turbo=false"), outpoint1.to_data());
}

BOOST_AUTO_TEST_CASE(native__address__unfunded__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/address/" + unfunded + "?format=data"), http::status::not_found);
}

// address/confirmed
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__address_confirmed__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/address/" + funded + "/confirmed?format=data"), outpoint1.to_data());
}

BOOST_AUTO_TEST_CASE(native__address_confirmed__unfunded__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/address/" + unfunded + "/confirmed?format=data"), http::status::not_found);
}

// address/unconfirmed
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__address_unconfirmed__any__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/address/" + funded + "/unconfirmed?format=data"), http::status::not_implemented);
}

// address/balance
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__address_balance__data__expected)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/address/" + funded + "/balance?format=data"), base16_chunk("00f2052a01"));
}

BOOST_AUTO_TEST_CASE(native__address_balance__text__expected)
{
    BOOST_REQUIRE_EQUAL(get_text("/v1/address/" + funded + "/balance?format=text"), "00f2052a01");
}

BOOST_AUTO_TEST_CASE(native__address_balance__json__expected)
{
    const auto response = get_json("/v1/address/" + funded + "/balance?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 5000000000);
}

BOOST_AUTO_TEST_CASE(native__address_balance__unfunded__zero)
{
    const auto response = get_json("/v1/address/" + unfunded + "/balance?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 0);
}

// address/subscribe
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__address_subscribe__data__outpoints)
{
    BOOST_REQUIRE_EQUAL(get_data("/v1/address/" + funded + "/subscribe?format=data"), outpoint1.to_data());
}

BOOST_AUTO_TEST_CASE(native__ws_address_subscribe__stop__empty)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_get_text("/v1/address/" + funded + "/subscribe?stop=true").empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(native_no_address_tests, native_no_address_setup_fixture)

BOOST_AUTO_TEST_CASE(native__address__disabled__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/address/" + funded + "?format=data"), http::status::not_implemented);
}

BOOST_AUTO_TEST_CASE(native__address_confirmed__disabled__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/address/" + funded + "/confirmed?format=data"), http::status::not_implemented);
}

BOOST_AUTO_TEST_CASE(native__address_balance__disabled__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/address/" + funded + "/balance?format=data"), http::status::not_implemented);
}

BOOST_AUTO_TEST_CASE(native__configuration__no_address__disabled)
{
    const auto response = get_json("/v1/configuration?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(!response.as_object().at("address").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
