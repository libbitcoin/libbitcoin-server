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
#include "../mocks/blocks.hpp"

BOOST_AUTO_TEST_SUITE(admin_query_tests)

using namespace network::rpc;
using namespace network::http;

// parse failures

BOOST_AUTO_TEST_CASE(parsers__admin_query__empty__false)
{
    request_t out{};
    BOOST_REQUIRE(!admin_query(out, {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__no_params_object__false)
{
    request_t out{};
    BOOST_REQUIRE(!admin_query(out, "/", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__invalid_uri_target__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!admin_query(out, "invalid uri with spaces?foo=bar", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__no_supported_media__unknown)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_plain };
    BOOST_REQUIRE(admin_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::unknown);
}

// strip_media

BOOST_AUTO_TEST_CASE(parsers__admin_query__no_query_no_accepts__unknown)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/", {}));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__no_query_accept_json__json)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(admin_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__no_query_accept_html__html)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_html };
    BOOST_REQUIRE(admin_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::text_html);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__no_query_accept_priority_json__json)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_html, media_type::application_json };
    BOOST_REQUIRE(admin_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__query_format_json__json)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?format=json", {}));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__query_format_html__html)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?format=html", {}));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::text_html);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__query_format_text__false)
{
    // Text is not a supported admin format.
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(!admin_query(out, "/?format=text", accepts));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__query_format_data__false)
{
    // Data is not a supported admin format.
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(!admin_query(out, "/?format=data", accepts));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__query_format_unknown__false)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(!admin_query(out, "/?format=foo", accepts));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__query_format_overrides_accept__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(admin_query(out, "/?format=html", accepts));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::text_html);
}

// filter

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_omitted__no_param)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?format=json", {}));

    const auto& params = std::get<object_t>(out.params.value());
    BOOST_REQUIRE(params.find(admin::token::filter) == params.end());
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_zero__expected)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?filter=0&format=json", {}));

    const auto& params = std::get<object_t>(out.params.value());
    const auto it = params.find(admin::token::filter);
    BOOST_REQUIRE(it != params.end());
    BOOST_REQUIRE(std::holds_alternative<uint64_t>(it->second.value()));
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(it->second.value()), 0u);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_value__expected)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?filter=123&format=json", {}));

    const auto& params = std::get<object_t>(out.params.value());
    const auto it = params.find(admin::token::filter);
    BOOST_REQUIRE(it != params.end());
    BOOST_REQUIRE(std::holds_alternative<uint64_t>(it->second.value()));
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(it->second.value()), 123u);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_maximum__expected)
{
    // 2^53 - 1 is the maximum exact json number.
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?filter=9007199254740991&format=json", {}));

    const auto& params = std::get<object_t>(out.params.value());
    const auto it = params.find(admin::token::filter);
    BOOST_REQUIRE(it != params.end());
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(it->second.value()), 9007199254740991u);
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_excess__false)
{
    // 2^53 exceeds the maximum exact json number.
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!admin_query(out, "/?filter=9007199254740992&format=json", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_non_numeric__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!admin_query(out, "/?filter=abc&format=json", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_leading_zero__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!admin_query(out, "/?filter=01&format=json", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_empty__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!admin_query(out, "/?filter=&format=json", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__filter_negative__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!admin_query(out, "/?filter=-1&format=json", {}));
}

BOOST_AUTO_TEST_CASE(parsers__admin_query__multiple_params__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, "/?filter=7&format=json", {}));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::application_json);

    const auto& params = std::get<object_t>(out.params.value());
    BOOST_REQUIRE(params.find(admin::token::filter) != params.end());
}

// http request overload

BOOST_AUTO_TEST_CASE(parsers__admin_query__http_request_overload_with_accept_json__json)
{
    network::http::request request{};
    request.target("/?filter=1");
    request.set(field::accept, "application/json");

    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(admin_query(out, request));
    BOOST_REQUIRE_EQUAL(strip_media(out), media_type::application_json);
}

// strip_media

BOOST_AUTO_TEST_CASE(parsers__strip_media__no_params__unknown)
{
    request_t model{};
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__strip_media__no_media_key__unknown)
{
    request_t model{};
    model.params = object_t{};
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__strip_media__invalid_type__unknown)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ true }; // not uint8_t
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__strip_media__json__json_and_erased)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::application_json) };
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::application_json);

    // The media param is stripped from the model (undeclared in interface).
    BOOST_REQUIRE(params.find("media") == params.end());
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__strip_media__html__html_and_erased)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::text_html) };
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::text_html);
    BOOST_REQUIRE(params.find("media") == params.end());
}

BOOST_AUTO_TEST_CASE(parsers__strip_media__unsupported_value__unknown_not_erased)
{
    // Unsupported media is not stripped (dispatch rejection preserved).
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::text_plain) };
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::unknown);
    BOOST_REQUIRE(params.find("media") != params.end());
}

BOOST_AUTO_TEST_CASE(parsers__strip_media__unknown_value__unknown)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ 99_u8 };
    BOOST_REQUIRE_EQUAL(strip_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_SUITE_END()
