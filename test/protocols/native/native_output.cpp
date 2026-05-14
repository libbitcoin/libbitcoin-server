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

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

using namespace system;
using namespace boost::beast;

// inscription (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__inscription__unknown_tx__not_found)
{
    const auto status = get_status(
        "/v1/inscription/0000000000000000000000000000000000000000000000000000000000000000/0?format=json");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__inscription__unknown_tx__not_found_text)
{
    const auto status = get_status(
        "/v1/inscription/0000000000000000000000000000000000000000000000000000000000000000/0?format=text");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__inscription__unknown_tx__not_found_data)
{
    const auto status = get_status(
        "/v1/inscription/0000000000000000000000000000000000000000000000000000000000000000/0?format=data");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()
