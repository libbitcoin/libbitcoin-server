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
#include "../test.hpp"

BC_AUTO_TEST_SUITE(bitcoind_target_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;
using media_type = network::http::media_type;

constexpr auto expected_hash = base16_hash(
    "0000000000000000000000000000000000000000000000000000000000000042");
static const std::string test_hash = encode_hash(expected_hash);

static const object_t& params_of(const request_t& request) NOEXCEPT
{
    BC_REQUIRE(request.params.has_value());
    BC_REQUIRE(std::holds_alternative<object_t>(request.params.value()));
    return std::get<object_t>(request.params.value());
}

static hash_cptr hash_of(const object_t& object) NOEXCEPT
{
    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());
    const auto cptr = any.get<const hash_digest>();
    BC_REQUIRE(cptr);
    return cptr;
}

static uint8_t media_of(const object_t& object) NOEXCEPT
{
    return std::get<uint8_t>(object.at("media").value());
}

// general

BC_AUTO_TEST_CASE(parsers__bitcoind_target__valid__jsonrpc_v2_named_params)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/chaininfo.json"));
    BC_REQUIRE(out.jsonrpc == network::rpc::version::v2);
    BC_REQUIRE(std::holds_alternative<object_t>(out.params.value()));
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__error_paths__expected)
{
    const std::vector<std::pair<std::string, code>> cases
    {
        { "", server::error::empty_path },
        { "?foo=bar", server::error::empty_path },
        { "/rest", server::error::invalid_target },
        { "/rest/bogus", server::error::invalid_target },
        { "/bogus", server::error::invalid_target },
        { "/rest/block", server::error::missing_hash },
        { "/rest/block/" + test_hash, server::error::invalid_target },
        { "/rest/block/" + test_hash + ".txt", server::error::invalid_target },
        { "/rest/block/nothex.json", server::error::invalid_hash },
        { "/rest/block/notxdetails", server::error::missing_hash },
        { "/rest/getutxos", server::error::missing_target },
        { "/rest/getutxos/checkmempool", server::error::missing_target },
        { "/rest/getutxos/nothex-0.json", server::error::invalid_hash },
        { "/rest/getutxos/" + test_hash + "-abc.json", server::error::invalid_number },
        { "/rest/getutxos/" + test_hash + ".json", server::error::invalid_hash },
        { "/rest/spenttxouts", server::error::missing_hash },
        { "/rest/spenttxouts/nothex.json", server::error::invalid_hash },
        { "/rest/blockhashbyheight", server::error::missing_target },
        { "/rest/blockhashbyheight/abc.json", server::error::invalid_number },
        { "/rest/blockhashbyheight/01.json", server::error::invalid_number },
        { "/rest/headers", server::error::missing_target },
        { "/rest/headers/abc/" + test_hash + ".json", server::error::invalid_number },
        { "/rest/headers/3", server::error::invalid_target },
        { "/rest/headers/3/nothex.json", server::error::invalid_hash },
        { "/rest/deploymentinfo/nothex.json", server::error::invalid_hash },
        { "/rest/deploymentinfo/" + test_hash + ".bin", server::error::invalid_target },
        { "/rest/blockfilter", server::error::missing_target },
        { "/rest/blockfilter/extended/" + test_hash + ".json", server::error::invalid_target },
        { "/rest/blockfilter/basic", server::error::missing_hash },
        { "/rest/blockfilterheaders/basic", server::error::missing_hash },
        { "/rest/blockpart", server::error::missing_hash },
        { "/rest/blockpart/nothex.bin", server::error::invalid_hash },
        { "/rest/blockpart/" + test_hash, server::error::invalid_target }
    };

    for (const auto& [path, expected]: cases)
    {
        request_t out{};
        BC_REQUIRE_MESSAGE(bitcoind_target(out, path) == expected, path);
    }
}

// chaininfo

BC_AUTO_TEST_CASE(parsers__bitcoind_target__chaininfo_variants__chain_information)
{
    const std::vector<std::string> paths
    {
        "/rest/chaininfo.json", "/rest/chaininfo", "/chaininfo.json", "chaininfo"
    };

    for (const auto& path: paths)
    {
        request_t out{};
        BC_REQUIRE_MESSAGE(!bitcoind_target(out, path), path);
        BC_REQUIRE_EQUAL(out.method, "chain_information");
        BC_REQUIRE(params_of(out).empty());
    }
}

// block

BC_AUTO_TEST_CASE(parsers__bitcoind_target__block_json__expected)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/block/" + test_hash + ".json"));
    BC_REQUIRE_EQUAL(out.method, "block");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 2u);
    BC_REQUIRE_EQUAL(media_of(object), to_value(media_type::application_json));
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__tx_json__expected)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/tx/" + test_hash + ".json"));
    BC_REQUIRE_EQUAL(out.method, "tx");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 2u);
    BC_REQUIRE_EQUAL(media_of(object), to_value(media_type::application_json));
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__block_media__mapped)
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
        const auto path = "/rest/block/" + test_hash + "." + extension;
        BC_REQUIRE_MESSAGE(!bitcoind_target(out, path), extension);
        BC_REQUIRE_EQUAL(media_of(params_of(out)), to_value(type));
    }
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__block_notxdetails__block_txs)
{
    request_t out{};
    const auto path = "/rest/block/notxdetails/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_txs");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 2u);
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__getutxos__outpoints)
{
    request_t out{};
    const auto path = "/rest/getutxos/checkmempool/" + test_hash + "-0/" + test_hash + "-7.json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "get_utxos");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(media_of(object), to_value(media_type::application_json));
    BC_REQUIRE_EQUAL(std::get<array_t>(object.at("outpoints").value()).size(), 2u);
    BC_REQUIRE(std::get<bool>(object.at("mempool").value()));
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__getutxos_unchecked__no_mempool)
{
    request_t out{};
    const auto path = "/rest/getutxos/" + test_hash + "-0.json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE(!params_of(out).contains("mempool"));
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__mempool_info__mempool_information)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/mempool/info.json"));
    BC_REQUIRE_EQUAL(out.method, "mempool_information");
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__mempool_contents__mempool)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/mempool/contents.json"));
    BC_REQUIRE_EQUAL(out.method, "mempool");
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__mempool_contents_hex__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/mempool/contents.hex"), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__mempool_missing__missing_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/mempool"), server::error::missing_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__spenttxouts__block_spent_tx_outputs)
{
    request_t out{};
    const auto path = "/rest/spenttxouts/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_spent_tx_outputs");
    BC_REQUIRE_EQUAL(*hash_of(params_of(out)), expected_hash);
}

// blockhashbyheight

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockhashbyheight__block_hash)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/blockhashbyheight/5.json"));
    BC_REQUIRE_EQUAL(out.method, "block_hash");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 2u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 5u);
    BC_REQUIRE_EQUAL(media_of(object), to_value(media_type::application_json));
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockhashbyheight_zero__block_hash)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/blockhashbyheight/0.json"));
    BC_REQUIRE_EQUAL(out.method, "block_hash");
    BC_REQUIRE_EQUAL(std::get<uint32_t>(params_of(out).at("height").value()), 0u);
}

// headers

BC_AUTO_TEST_CASE(parsers__bitcoind_target__headers__block_headers)
{
    request_t out{};
    const auto path = "/rest/headers/3/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_headers");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 3u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("count").value()), 3u);
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
    BC_REQUIRE_EQUAL(media_of(object), to_value(media_type::application_json));
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__headers_no_query__default_count)
{
    request_t out{};
    const auto path = "/rest/headers/" + test_hash + ".bin";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_headers");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("count").value()), 5u);
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
}

// deploymentinfo

BC_AUTO_TEST_CASE(parsers__bitcoind_target__deploymentinfo__no_hash)
{
    request_t out{};
    BC_REQUIRE(!bitcoind_target(out, "/rest/deploymentinfo.json"));
    BC_REQUIRE_EQUAL(out.method, "deployment_info");
    BC_REQUIRE(params_of(out).empty());
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__deploymentinfo_hash__deployment_info)
{
    request_t out{};
    const auto path = "/rest/deploymentinfo/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "deployment_info");
    BC_REQUIRE_EQUAL(*hash_of(params_of(out)), expected_hash);
}

// blockfilter

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilter_basic__block_filter)
{
    request_t out{};
    const auto path = "/rest/blockfilter/basic/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_filter");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 3u);
    BC_REQUIRE_EQUAL(std::get<uint8_t>(object.at("type").value()), 0u);
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_basic__block_filter_headers)
{
    request_t out{};
    const auto path = "/rest/blockfilterheaders/basic/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_filter_headers");
    BC_REQUIRE_EQUAL(std::get<uint8_t>(params_of(out).at("type").value()), 0u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(params_of(out).at("count").value()), 5u);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_count__legacy_form)
{
    request_t out{};
    const auto path = "/rest/blockfilterheaders/basic/7/" + test_hash + ".json";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_filter_headers");
    BC_REQUIRE_EQUAL(std::get<uint32_t>(params_of(out).at("count").value()), 7u);
    BC_REQUIRE_EQUAL(*hash_of(params_of(out)), expected_hash);
}

// blockpart

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockpart__block_part)
{
    request_t out{};
    const auto path = "/rest/blockpart/" + test_hash + ".bin";
    BC_REQUIRE(!bitcoind_target(out, path));
    BC_REQUIRE_EQUAL(out.method, "block_part");

    const auto& object = params_of(out);
    BC_REQUIRE_EQUAL(object.size(), 2u);
    BC_REQUIRE_EQUAL(*hash_of(object), expected_hash);
    BC_REQUIRE_EQUAL(media_of(object), to_value(media_type::application_octet_stream));
}

// error paths

BC_AUTO_TEST_CASE(parsers__bitcoind_target__tx_no_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/tx"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__tx_no_extension__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/tx/" + test_hash), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__tx_not_hex__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/tx/nothex.json"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockhashbyheight_no_extension__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockhashbyheight/5"), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilter_no_extension__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilter/basic/" + test_hash), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilter_not_hex__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilter/basic/nothex.json"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_no_type__missing_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilterheaders"), server::error::missing_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_extended__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilterheaders/extended/" + test_hash + ".json"), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_invalid_count__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilterheaders/basic/abc/" + test_hash + ".json"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_no_extension__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilterheaders/basic/" + test_hash), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__blockfilterheaders_not_hex__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/blockfilterheaders/basic/nothex.json"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__getutxos_no_extension__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/getutxos/" + test_hash + "-0"), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__bitcoind_target__spenttxouts_no_extension__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(bitcoind_target(out, "/rest/spenttxouts/" + test_hash), server::error::invalid_target);
}

BC_AUTO_TEST_SUITE_END()
