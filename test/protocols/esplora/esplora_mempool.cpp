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

// txs/package
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__broadcast_package__empty__bad_request)
{
    BOOST_REQUIRE_EQUAL(post_status("/txs/package", "[]"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__broadcast_package__malformed__bad_request)
{
    BOOST_REQUIRE_EQUAL(post_status("/txs/package", "[42]"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__broadcast_package__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(post_status("/txs/package", "[\"00\"]"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__invalid_target__not_found)
{
    const auto status = get_status("/bogus");
    BOOST_REQUIRE_EQUAL(status, http::status::not_found);
}

// request validation
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__get__non_origin_form__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("blocks/tip/height"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__get__opaque_origin__forbidden)
{
    BOOST_REQUIRE_EQUAL(get_status("/blocks/tip/height", http::field::origin, "null"), http::status::forbidden);
}

BOOST_AUTO_TEST_CASE(esplora__get__empty_host__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/blocks/tip/height", http::field::host, ""), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__post__non_origin_form__bad_request)
{
    BOOST_REQUIRE_EQUAL(post_status("tx", "00"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__post__opaque_origin__forbidden)
{
    BOOST_REQUIRE_EQUAL(post_status("/tx", "00", http::field::origin, "null"), http::status::forbidden);
}

BOOST_AUTO_TEST_CASE(esplora__post__empty_host__bad_request)
{
    BOOST_REQUIRE_EQUAL(post_status("/tx", "00", http::field::host, ""), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__post__invalid_target__not_found)
{
    BOOST_REQUIRE_EQUAL(post_status("/bogus", "00"), http::status::not_found);
}

BOOST_AUTO_TEST_CASE(esplora__post__bodyless_method__method_not_allowed)
{
    BOOST_REQUIRE_EQUAL(post_status("/blocks/tip/height", "00"), http::status::method_not_allowed);
}

BOOST_AUTO_TEST_SUITE_END()

// broadcast mempool
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(esplora_broadcast_tests, esplora_broadcast_setup_fixture)

BOOST_AUTO_TEST_CASE(esplora__broadcast_package__broadcast__success)
{
    const auto package = "[\"" + encode_base16(test::tx1c.to_data(true)) + "\"]";
    const auto response = boost::json::parse(post_text("/txs/package", package));
    BOOST_REQUIRE(response.is_object());

    const auto& result = response.as_object();
    BOOST_REQUIRE_EQUAL(result.at("package_msg").as_string(), "success");

    const auto& tx = result.at("tx-results").as_object().at(encode_hash(test::tx1c.hash(true))).as_object();
    BOOST_REQUIRE_EQUAL(tx.at("txid").as_string(), encode_hash(test::tx1c.hash(false)));
    BOOST_REQUIRE_EQUAL(tx.at("vsize").as_int64(), to_signed(test::tx1c.virtual_size()));

    const auto txids = get_json("/mempool/txids");
    BOOST_REQUIRE_EQUAL(txids.as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(esplora__mempool__broadcast__expected)
{
    const auto tx1c = encode_base16(test::tx1c.to_data(true));
    BOOST_REQUIRE_EQUAL(post_text("/tx", tx1c), encode_hash(test::tx1c.hash(false)));

    const auto response = get_json("/mempool");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    const auto vsize = to_signed(test::tx1c.virtual_size());
    BOOST_REQUIRE_EQUAL(object.at("count").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(object.at("vsize").as_int64(), vsize);
    BOOST_REQUIRE_EQUAL(object.at("total_fee").as_int64(), 10);

    const auto& histogram = object.at("fee_histogram").as_array();
    BOOST_REQUIRE_EQUAL(histogram.size(), 1u);
    BOOST_REQUIRE_EQUAL(histogram.front().as_array().at(0).as_double(), 10.0 / test::tx1c.virtual_size());
    BOOST_REQUIRE_EQUAL(histogram.front().as_array().at(1).as_int64(), vsize);
}

BOOST_AUTO_TEST_CASE(esplora__mempool_txids__broadcast__txid)
{
    const auto tx1c = encode_base16(test::tx1c.to_data(true));
    BOOST_REQUIRE_EQUAL(post_text("/tx", tx1c), encode_hash(test::tx1c.hash(false)));

    const auto response = get_json("/mempool/txids");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(response.as_array().front().as_string(), encode_hash(test::tx1c.hash(false)));
}

BOOST_AUTO_TEST_CASE(esplora__mempool_recent__broadcast__expected)
{
    const auto tx1c = encode_base16(test::tx1c.to_data(true));
    BOOST_REQUIRE_EQUAL(post_text("/tx", tx1c), encode_hash(test::tx1c.hash(false)));

    const auto response = get_json("/mempool/recent");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);

    const auto& tx = response.as_array().front().as_object();
    BOOST_REQUIRE_EQUAL(tx.at("txid").as_string(), encode_hash(test::tx1c.hash(false)));
    BOOST_REQUIRE_EQUAL(tx.at("fee").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(tx.at("vsize").as_int64(), to_signed(test::tx1c.virtual_size()));
    BOOST_REQUIRE_EQUAL(tx.at("value").as_int64(), 0x5a);
}

BOOST_AUTO_TEST_SUITE_END()
