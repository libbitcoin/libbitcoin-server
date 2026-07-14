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

BOOST_AUTO_TEST_SUITE(native_query_tests)

using namespace network::rpc;
using namespace network::http;

BOOST_AUTO_TEST_CASE(parsers__native_query__empty__false)
{
    request_t out{};
    BOOST_REQUIRE(!native_query(out, {}));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_params_object__false)
{
    request_t out{};
    BOOST_REQUIRE(!native_query(out, "/", {}));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__invalid_uri_target__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!native_query(out, "invalid uri with spaces?foo=bar", {}));
}

// valid parse doesn't require acceptable.
BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_no_accepts__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, "/", {}));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_json__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_html__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_html };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::text_html);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_text__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_plain };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::text_plain);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_data__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_octet_stream };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_octet_stream);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_priority_json__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_html, media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_priority_html__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_plain, media_type::text_html };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::text_html);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_priority_text__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_octet_stream, media_type::text_plain };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::text_plain);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_accept_priority_data__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_octet_stream };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_octet_stream);
}

// valid parse doesn't require acceptable.
BOOST_AUTO_TEST_CASE(parsers__native_query__no_query_no_matching_accept__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::unknown };
    BOOST_REQUIRE(native_query(out, "/", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_json__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, "/?format=json", {}));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_html__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, "/?format=html", {}));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::text_html);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_text__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, "/?format=text", {}));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::text_plain);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_data__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, "/?format=data", {}));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_octet_stream);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_unknown__false)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(!native_query(out, "/?format=foo", accepts));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_xml__false)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(!native_query(out, "/?format=xml", accepts));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__query_format_overrides_accept__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::text_html };
    BOOST_REQUIRE(native_query(out, "/?format=json", accepts));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__native_query__witness_false__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/?witness=false", accepts));

    const auto& params = std::get<object_t>(out.params.value());
    const auto it = params.find(native::token::witness);
    BOOST_REQUIRE(it != params.end());
    BOOST_REQUIRE(std::holds_alternative<bool>(it->second.value()));
    BOOST_REQUIRE(!std::get<bool>(it->second.value()));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__witness_true__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/?witness=true", accepts));

    const auto& params = std::get<object_t>(out.params.value());
    BOOST_REQUIRE(params.find(native::token::witness) == params.end());
}

BOOST_AUTO_TEST_CASE(parsers__native_query__turbo_false__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/?turbo=false", accepts));

    const auto& params = std::get<object_t>(out.params.value());
    const auto it = params.find(native::token::turbo);
    BOOST_REQUIRE(it != params.end());
    BOOST_REQUIRE(std::holds_alternative<bool>(it->second.value()));
    BOOST_REQUIRE(!std::get<bool>(it->second.value()));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__turbo_true__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/?turbo=true", accepts));

    const auto& params = std::get<object_t>(out.params.value());
    BOOST_REQUIRE(params.find(native::token::turbo) == params.end());
}

BOOST_AUTO_TEST_CASE(parsers__native_query__stop_true__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/?stop=true", accepts));

    const auto& params = std::get<object_t>(out.params.value());
    const auto it = params.find(native::token::stop);
    BOOST_REQUIRE(it != params.end());
    BOOST_REQUIRE(std::holds_alternative<bool>(it->second.value()));
    BOOST_REQUIRE(std::get<bool>(it->second.value()));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__stop_false__true)
{
    request_t out{};
    out.params = object_t{};
    const media_types accepts{ media_type::application_json };
    BOOST_REQUIRE(native_query(out, "/?stop=false", accepts));

    const auto& params = std::get<object_t>(out.params.value());
    BOOST_REQUIRE(params.find(native::token::stop) == params.end());
}

BOOST_AUTO_TEST_CASE(parsers__native_query__stop_invalid__false)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(!native_query(out, "/?stop=foo", {}));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__multiple_params__true)
{
    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, "/?witness=false&turbo=true&stop=true&format=json", {}));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_json);

    const auto& params = std::get<object_t>(out.params.value());
    BOOST_REQUIRE(params.find("witness") != params.end());
    BOOST_REQUIRE(params.find("stop") != params.end());
    BOOST_REQUIRE(params.find("turbo") == params.end());
}

BOOST_AUTO_TEST_CASE(parsers__native_query__http_request_overload_empty__false)
{
    request_t out{};
    BOOST_REQUIRE(!native_query(out, {}));
}

BOOST_AUTO_TEST_CASE(parsers__native_query__http_request_overload_with_accept_json__true)
{
    network::http::request request{};
    request.target("/");
    request.set(field::accept, "application/json");

    request_t out{};
    out.params = object_t{};
    BOOST_REQUIRE(native_query(out, request));
    BOOST_REQUIRE_EQUAL(get_media(out), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__unknown_no_params__unknown)
{
    request_t model{};
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__unknown_no_media_key__unknown)
{
    request_t model{};
    model.params = object_t{};
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__unknown_invalid_type__unknown)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ true }; // not uint8_t
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__json__json)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::application_json) };
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::application_json);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__html__html)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::text_html) };
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::text_html);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__text__text)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::text_plain) };
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::text_plain);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__data__data)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ static_cast<uint8_t>(media_type::application_octet_stream) };
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::application_octet_stream);
}

BOOST_AUTO_TEST_CASE(parsers__get_media__unknown_value__unknown)
{
    request_t model{};
    model.params = object_t{};
    auto& params = std::get<object_t>(model.params.value());
    params["media"] = value_t{ 99_u8 };
    BOOST_REQUIRE_EQUAL(get_media(model), media_type::unknown);
}

BOOST_AUTO_TEST_SUITE_END()
