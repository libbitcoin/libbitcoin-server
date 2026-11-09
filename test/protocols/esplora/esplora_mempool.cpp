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
namespace http = network::http;
namespace ws = network::ws;

BC_FIXTURE_TEST_SUITE(esplora_tests, esplora_ten_block_setup_fixture)

// mempool
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__mempool__json__expected)
{
    const auto response = get_json("/mempool");
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BC_REQUIRE_EQUAL(object.size(), 4u);
    BC_REQUIRE_EQUAL(object.at("count").as_int64(), 0);
    BC_REQUIRE_EQUAL(object.at("vsize").as_int64(), 0);
    BC_REQUIRE_EQUAL(object.at("total_fee").as_int64(), 0);
    BC_REQUIRE(object.at("fee_histogram").as_array().empty());
}

BC_AUTO_TEST_CASE(esplora__mempool_txids__json__empty)
{
    const auto response = get_json("/mempool/txids");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

BC_AUTO_TEST_CASE(esplora__mempool_recent__json__empty)
{
    const auto response = get_json("/mempool/recent");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

BC_AUTO_TEST_CASE(esplora__mempool_invalid_component__not_found)
{
    const auto status = get_status("/mempool/bogus");
    BC_REQUIRE_EQUAL(status, http::status::not_found);
}

// fee-estimates
// ----------------------------------------------------------------------------

// The store is below the estimation horizon, so all targets are omitted.
BC_AUTO_TEST_CASE(esplora__fee_estimates__json__empty)
{
    const auto response = get_json("/fee-estimates");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE(response.as_object().empty());
}

BC_AUTO_TEST_CASE(esplora__fee_estimates__extra_segment__not_found)
{
    const auto status = get_status("/fee-estimates/extra");
    BC_REQUIRE_EQUAL(status, http::status::not_found);
}

// txs/package
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__broadcast_package__empty__bad_request)
{
    BC_REQUIRE_EQUAL(post_status("/txs/package", "[]"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__broadcast_package__malformed__bad_request)
{
    BC_REQUIRE_EQUAL(post_status("/txs/package", "[42]"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__broadcast_package__undecodable__bad_request)
{
    BC_REQUIRE_EQUAL(post_status("/txs/package", "[\"00\"]"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__invalid_target__not_found)
{
    const auto status = get_status("/bogus");
    BC_REQUIRE_EQUAL(status, http::status::not_found);
}

// request validation
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__get__non_origin_form__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("blocks/tip/height"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__get__opaque_origin__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/blocks/tip/height", http::field::origin, "null"), http::status::forbidden);
}

BC_AUTO_TEST_CASE(esplora__get__origin__access_control)
{
    const auto response = get_response("/blocks/tip/height", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::access_control_allow_origin], "http://example.com");
    BC_REQUIRE_EQUAL(response[http::field::vary], "origin");
}

BC_AUTO_TEST_CASE(esplora__get__empty_host__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/blocks/tip/height", http::field::host, ""), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__post__non_origin_form__bad_request)
{
    BC_REQUIRE_EQUAL(post_status("tx", "00"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__post__opaque_origin__forbidden)
{
    BC_REQUIRE_EQUAL(post_status("/tx", "00", http::field::origin, "null"), http::status::forbidden);
}

BC_AUTO_TEST_CASE(esplora__post__empty_host__bad_request)
{
    BC_REQUIRE_EQUAL(post_status("/tx", "00", http::field::host, ""), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__post__invalid_target__not_found)
{
    BC_REQUIRE_EQUAL(post_status("/bogus", "00"), http::status::not_found);
}

BC_AUTO_TEST_CASE(esplora__post__bodyless_method__method_not_allowed)
{
    BC_REQUIRE_EQUAL(post_status("/blocks/tip/height", "00"), http::status::method_not_allowed);
}

BC_AUTO_TEST_SUITE_END()

// broadcast mempool
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(esplora_broadcast_tests, esplora_broadcast_setup_fixture)

BC_AUTO_TEST_CASE(esplora__broadcast_package__broadcast__success)
{
    const auto package = "[\"" + encode_base16(test::tx1c.to_data(true)) + "\"]";
    const auto response = system::json::parse(post_text("/txs/package", package));
    BC_REQUIRE(response.is_object());

    const auto& result = response.as_object();
    BC_REQUIRE_EQUAL(result.at("package_msg").as_string(), "success");

    const auto& tx = result.at("tx-results").as_object().at(encode_hash(test::tx1c.hash(true))).as_object();
    BC_REQUIRE_EQUAL(tx.at("txid").as_string(), encode_hash(test::tx1c.hash(false)));
    BC_REQUIRE_EQUAL(tx.at("vsize").as_int64(), to_signed(test::tx1c.virtual_size()));

    const auto txids = get_json("/mempool/txids");
    BC_REQUIRE_EQUAL(txids.as_array().size(), 1u);
}

BC_AUTO_TEST_CASE(esplora__mempool__broadcast__expected)
{
    const auto tx1c = encode_base16(test::tx1c.to_data(true));
    BC_REQUIRE_EQUAL(post_text("/tx", tx1c), encode_hash(test::tx1c.hash(false)));

    const auto response = get_json("/mempool");
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    const auto vsize = to_signed(test::tx1c.virtual_size());
    BC_REQUIRE_EQUAL(object.at("count").as_int64(), 1);
    BC_REQUIRE_EQUAL(object.at("vsize").as_int64(), vsize);
    BC_REQUIRE_EQUAL(object.at("total_fee").as_int64(), 10);

    const auto& histogram = object.at("fee_histogram").as_array();
    BC_REQUIRE_EQUAL(histogram.size(), 1u);
    BC_REQUIRE_EQUAL(histogram.front().as_array().at(0).as_double(), 10.0 / test::tx1c.virtual_size());
    BC_REQUIRE_EQUAL(histogram.front().as_array().at(1).as_int64(), vsize);
}

BC_AUTO_TEST_CASE(esplora__mempool_txids__broadcast__txid)
{
    const auto tx1c = encode_base16(test::tx1c.to_data(true));
    BC_REQUIRE_EQUAL(post_text("/tx", tx1c), encode_hash(test::tx1c.hash(false)));

    const auto response = get_json("/mempool/txids");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(response.as_array().front().as_string(), encode_hash(test::tx1c.hash(false)));
}

BC_AUTO_TEST_CASE(esplora__mempool_recent__broadcast__expected)
{
    const auto tx1c = encode_base16(test::tx1c.to_data(true));
    BC_REQUIRE_EQUAL(post_text("/tx", tx1c), encode_hash(test::tx1c.hash(false)));

    const auto response = get_json("/mempool/recent");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);

    const auto& tx = response.as_array().front().as_object();
    BC_REQUIRE_EQUAL(tx.at("txid").as_string(), encode_hash(test::tx1c.hash(false)));
    BC_REQUIRE_EQUAL(tx.at("fee").as_int64(), 10);
    BC_REQUIRE_EQUAL(tx.at("vsize").as_int64(), to_signed(test::tx1c.virtual_size()));
    BC_REQUIRE_EQUAL(tx.at("value").as_int64(), 0x5a);
}

BC_AUTO_TEST_SUITE_END()

// to_fee_summary

BC_AUTO_TEST_SUITE(esplora_summary_tests)

BC_AUTO_TEST_CASE(esplora__to_fee_summary__empty__zeros)
{
    const auto summary = to_fee_summary({}, 100);
    BC_REQUIRE_EQUAL(summary.count, 0u);
    BC_REQUIRE_EQUAL(summary.vsize, 0u);
    BC_REQUIRE_EQUAL(summary.fee, 0u);
    BC_REQUIRE(summary.histogram.empty());
}

BC_AUTO_TEST_CASE(esplora__to_fee_summary__rates__totals_and_histogram)
{
    const database::fee_rates rates
    {
        { 60, 600 },
        { 300, 600 },
        { 50, 50 }
    };

    const auto summary = to_fee_summary(rates, 100);
    BC_REQUIRE_EQUAL(summary.count, 3u);
    BC_REQUIRE_EQUAL(summary.vsize, 410u);
    BC_REQUIRE_EQUAL(summary.fee, 1250u);
    BC_REQUIRE(summary.histogram == to_fee_histogram(rates, 100));
}

BC_AUTO_TEST_SUITE_END()
