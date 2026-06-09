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
#include "../test.hpp"

BOOST_AUTO_TEST_SUITE(bitcoind_target_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;
using media_type = network::http::media_type;

// A valid display-order hash whose internal (reversed) value is 0x42.
static const std::string test_hash =
    "0000000000000000000000000000000000000000000000000000000000000042";

// Extract the parsed params object (asserts the request carries one).
static const object_t& params_of(const request_t& request) NOEXCEPT
{
    BOOST_REQUIRE(request.params.has_value());
    BOOST_REQUIRE(std::holds_alternative<object_t>(request.params.value()));
    return std::get<object_t>(request.params.value());
}

// Extract the "hash" param as a hash_cptr (asserts presence and type).
static hash_cptr hash_of(const object_t& object) NOEXCEPT
{
    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());
    const auto cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(cptr);
    return cptr;
}

static uint8_t media_of(const object_t& object) NOEXCEPT
{
    return std::get<uint8_t>(object.at("media").value());
}

// General structure
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__valid__jsonrpc_v2_named_params)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out, "/rest/chaininfo.json"));
    BOOST_REQUIRE(out.jsonrpc == network::rpc::version::v2);
    BOOST_REQUIRE(std::holds_alternative<object_t>(out.params.value()));
}

// Error paths (data-driven: path -> expected error code).
BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__error_paths__expected)
{
    const std::vector<std::pair<std::string, code>> cases
    {
        // general
        { "", server::error::empty_path },
        { "?foo=bar", server::error::empty_path },
        { "/rest", server::error::missing_target },
        { "/rest/bogus", server::error::invalid_target },
        { "/bogus", server::error::invalid_target },

        // block
        { "/rest/block", server::error::missing_hash },
        { "/rest/block/" + test_hash, server::error::invalid_target },
        { "/rest/block/" + test_hash + ".txt", server::error::invalid_target },
        { "/rest/block/nothex.json", server::error::invalid_hash },
        { "/rest/block/notxdetails", server::error::missing_hash },
        { "/rest/block/spent", server::error::missing_hash },

        // blockhashbyheight
        { "/rest/blockhashbyheight", server::error::missing_target },
        { "/rest/blockhashbyheight/abc.json", server::error::invalid_number },
        { "/rest/blockhashbyheight/01.json", server::error::invalid_number },

        // headers
        { "/rest/headers", server::error::missing_target },
        { "/rest/headers/abc/" + test_hash + ".json", server::error::invalid_number },
        { "/rest/headers/3", server::error::missing_hash },
        { "/rest/headers/3/nothex.json", server::error::invalid_hash },

        // blockfilter / blockfilterheaders
        { "/rest/blockfilter", server::error::missing_target },
        { "/rest/blockfilter/extended/" + test_hash + ".json", server::error::invalid_target },
        { "/rest/blockfilter/basic", server::error::missing_hash },
        { "/rest/blockfilterheaders/basic", server::error::missing_hash },

        // blockpart
        { "/rest/blockpart", server::error::missing_hash },
        { "/rest/blockpart/nothex/0/80.bin", server::error::invalid_hash },
        { "/rest/blockpart/" + test_hash, server::error::missing_target },
        { "/rest/blockpart/" + test_hash + "/abc/80.bin", server::error::invalid_number },
        { "/rest/blockpart/" + test_hash + "/0", server::error::missing_target },
        { "/rest/blockpart/" + test_hash + "/0/abc.bin", server::error::invalid_number }
    };

    for (const auto& [path, expected]: cases)
    {
        request_t out{};
        BOOST_REQUIRE_MESSAGE(bitcoind_target(out, path) == expected, path);
    }
}

// chaininfo
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__chaininfo_variants__chain_information)
{
    // Optional "rest" prefix and optional ".json" extension are both accepted.
    const std::vector<std::string> paths
    {
        "/rest/chaininfo.json", "/rest/chaininfo", "/chaininfo.json", "chaininfo"
    };

    for (const auto& path: paths)
    {
        request_t out{};
        BOOST_REQUIRE_MESSAGE(!bitcoind_target(out, path), path);
        BOOST_REQUIRE_EQUAL(out.method, "chain_information");
        BOOST_REQUIRE(params_of(out).empty());
    }
}

// block
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__block_json__expected)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out, "/rest/block/" + test_hash + ".json"));
    BOOST_REQUIRE_EQUAL(out.method, "block");

    const auto& object = params_of(out);
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(media_of(object),
        to_value(media_type::application_json));
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_of(object)), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__block_media__mapped)
{
    const std::vector<std::pair<std::string, media_type>> cases
    {
        { "json", media_type::application_json },
        { "hex", media_type::text_plain },
        { "bin", media_type::application_octet_stream }
    };

    for (const auto& [extension, type]: cases)
    {
        request_t out{};
        BOOST_REQUIRE_MESSAGE(!bitcoind_target(out,
            "/rest/block/" + test_hash + "." + extension), extension);
        BOOST_REQUIRE_EQUAL(media_of(params_of(out)), to_value(type));
    }
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__block_notxdetails__block_txs)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out,
        "/rest/block/notxdetails/" + test_hash + ".json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_txs");

    const auto& object = params_of(out);
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_of(object)), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__block_spent__block_spent_tx_outputs)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out,
        "/rest/block/spent/" + test_hash + ".json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_spent_tx_outputs");
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_of(params_of(out))), uint256_t{ 0x42 });
}

// blockhashbyheight
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__blockhashbyheight__block_hash)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out, "/rest/blockhashbyheight/5.json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_hash");

    const auto& object = params_of(out);
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 5u);
    BOOST_REQUIRE_EQUAL(media_of(object),
        to_value(media_type::application_json));
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__blockhashbyheight_zero__block_hash)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out, "/rest/blockhashbyheight/0.json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_hash");
    BOOST_REQUIRE_EQUAL(
        std::get<uint32_t>(params_of(out).at("height").value()), 0u);
}

// headers
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__headers__block_headers)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out,
        "/rest/headers/3/" + test_hash + ".json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_headers");

    const auto& object = params_of(out);
    BOOST_REQUIRE_EQUAL(object.size(), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("count").value()), 3u);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_of(object)), uint256_t{ 0x42 });
    BOOST_REQUIRE_EQUAL(media_of(object),
        to_value(media_type::application_json));
}

// blockfilter / blockfilterheaders
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilter_basic__block_filter)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out,
        "/rest/blockfilter/basic/" + test_hash + ".json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_filter");

    const auto& object = params_of(out);
    BOOST_REQUIRE_EQUAL(object.size(), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("type").value()), 0u);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_of(object)), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_basic__block_filter_headers)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out,
        "/rest/blockfilterheaders/basic/" + test_hash + ".json"));
    BOOST_REQUIRE_EQUAL(out.method, "block_filter_headers");
    BOOST_REQUIRE_EQUAL(
        std::get<uint8_t>(params_of(out).at("type").value()), 0u);
}

// blockpart
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(parsers__bitcoind_target__blockpart__block_part)
{
    request_t out{};
    BOOST_REQUIRE(!bitcoind_target(out,
        "/rest/blockpart/" + test_hash + "/0/80.bin"));
    BOOST_REQUIRE_EQUAL(out.method, "block_part");

    const auto& object = params_of(out);
    BOOST_REQUIRE_EQUAL(object.size(), 4u);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("offset").value()), 0u);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("size").value()), 80u);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_of(object)), uint256_t{ 0x42 });
    BOOST_REQUIRE_EQUAL(media_of(object),
        to_value(media_type::application_octet_stream));
}

BOOST_AUTO_TEST_SUITE_END()
