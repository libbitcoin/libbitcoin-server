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
#include "esplora_setup_fixture.hpp"

using namespace system;
using namespace boost::beast;

BOOST_FIXTURE_TEST_SUITE(esplora_tests, esplora_ten_block_setup_fixture)

// mempool
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__mempool__json__expected)
{
    const auto response = get_json("/mempool");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.size(), 4u);
    BOOST_REQUIRE_EQUAL(object.at("count").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(object.at("vsize").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(object.at("total_fee").as_int64(), 0);
    BOOST_REQUIRE(object.at("fee_histogram").as_array().empty());
}

BOOST_AUTO_TEST_CASE(esplora__mempool_txids__json__empty)
{
    const auto response = get_json("/mempool/txids");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE(response.as_array().empty());
}

BOOST_AUTO_TEST_CASE(esplora__mempool_recent__json__empty)
{
    const auto response = get_json("/mempool/recent");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE(response.as_array().empty());
}

BOOST_AUTO_TEST_CASE(esplora__mempool_invalid_component__not_found)
{
    const auto status = get_status("/mempool/bogus");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

// fee-estimates
// ----------------------------------------------------------------------------

// The store is below the estimation horizon, so all targets are omitted.
BOOST_AUTO_TEST_CASE(esplora__fee_estimates__json__empty)
{
    const auto response = get_json("/fee-estimates");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().empty());
}

BOOST_AUTO_TEST_CASE(esplora__fee_estimates__extra_segment__not_found)
{
    const auto status = get_status("/fee-estimates/extra");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

// unimplemented
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__broadcast_package__unsubscribed__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/txs/package"), http::status::not_implemented);
}

BOOST_AUTO_TEST_CASE(esplora__invalid_target__not_found)
{
    const auto status = get_status("/bogus");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

BOOST_AUTO_TEST_SUITE_END()
