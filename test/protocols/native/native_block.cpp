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
#include "native_setup_fixture.hpp"

using namespace system;
using namespace boost::beast;

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// top (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__top__json__expected)
{
    const auto response = get_json("/v1/top?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__top__text__expected)
{
    const auto body = get_text("/v1/top?format=text");
    BOOST_REQUIRE_EQUAL(body, "09");
}

BOOST_AUTO_TEST_CASE(native__top__data__expected)
{
    const auto body = get_data("/v1/top?format=data");
    BOOST_REQUIRE_EQUAL(body, base16_chunk("09"));
}

BOOST_AUTO_TEST_CASE(native__top__xml__not_acceptable)
{
    const auto status = get_status("/v1/top?format=xml");
    BOOST_REQUIRE_EQUAL(status, http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__top__default__not_acceptable)
{
    const auto status = get_status("/v1/top?format=xml");
    BOOST_REQUIRE_EQUAL(status, http::status::not_acceptable);
}

// subscribe

BOOST_AUTO_TEST_CASE(native__top_subscribe__json__expected)
{
    const auto response = get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

// top (websockets)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__ws_upgrade__always__expected)
{
    const auto ec = ws_upgrade();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
}

BOOST_AUTO_TEST_CASE(native__ws_top__json__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/top?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__ws_top__text__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/top?format=text");
    BOOST_REQUIRE_EQUAL(response, "09");
}

BOOST_AUTO_TEST_CASE(native__ws_top__data__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_data("/v1/top?format=data");
    BOOST_REQUIRE_EQUAL(response, base16_chunk("09"));
}

BOOST_AUTO_TEST_CASE(native__ws_top__xml__error_eof)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_dropped("/v1/top?format=xml"));
}

// subscribe

BOOST_AUTO_TEST_CASE(native__ws_top_subscribe__json__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__ws_top_subscribe__stop__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/top/subscribe?stop=true");
    BOOST_REQUIRE(response.empty());
}

BOOST_AUTO_TEST_SUITE_END()
