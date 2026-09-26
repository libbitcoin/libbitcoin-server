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

using namespace boost::beast;

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// configuration
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__configuration__json__expected)
{
    const auto response = get_json("/v1/configuration?format=json");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.size(), 7u);
    BOOST_REQUIRE(object.at("address").as_bool());
    BOOST_REQUIRE(object.at("filter").as_bool());
    BOOST_REQUIRE(!object.at("turbo").as_bool());
    BOOST_REQUIRE(object.at("witness").as_bool());
    BOOST_REQUIRE(object.at("retarget").as_bool());
    BOOST_REQUIRE(object.at("difficult").as_bool());
    BOOST_REQUIRE_EQUAL(object.at("pruned").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(native__configuration__text__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/configuration?format=text"), http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__configuration__data__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/configuration?format=data"), http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__ws_configuration__default__json)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/configuration");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().at("witness").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
