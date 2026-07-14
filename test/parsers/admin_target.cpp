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

BOOST_AUTO_TEST_SUITE(admin_target_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;

// General errors

BOOST_AUTO_TEST_CASE(parsers__admin_target__empty_path__empty_path)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "?foo=bar"), server::error::empty_path);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__missing_version__missing_version)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/"), server::error::missing_version);
    BOOST_REQUIRE_EQUAL(admin_target(out, "/log/subscribe"), server::error::missing_version);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__invalid_version__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/vinvalid/log/subscribe"), server::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__version_leading_zero__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v01/log/subscribe"), server::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__missing_target__missing_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3"), server::error::missing_target);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__invalid_target__invalid_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/invalid"), server::error::invalid_target);
}

// log/subscribe

BOOST_AUTO_TEST_CASE(parsers__admin_target__log_subscribe_valid__expected)
{
    const std::string path = "/v42/log/subscribe";

    request_t request{};
    BOOST_REQUIRE(!admin_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "log_subscribe");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__log_subscribe_sloppy_valid__expected)
{
    const std::string path = "//v255//log//subscribe//?foo=bar";

    request_t request{};
    BOOST_REQUIRE(!admin_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "log_subscribe");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 255u);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__log_missing_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/log"), server::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__log_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/log/invalid"), server::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__log_subscribe_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/log/subscribe/extra"), server::error::extra_segment);
}

// event/subscribe

BOOST_AUTO_TEST_CASE(parsers__admin_target__event_subscribe_valid__expected)
{
    const std::string path = "/v42/event/subscribe";

    request_t request{};
    BOOST_REQUIRE(!admin_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "event_subscribe");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BOOST_REQUIRE_EQUAL(version, 42u);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__event_missing_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/event"), server::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__event_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/event/invalid"), server::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parsers__admin_target__event_subscribe_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/event/subscribe/extra"), server::error::extra_segment);
}

// Cross-interface targets (native grammar is not admin grammar).

BOOST_AUTO_TEST_CASE(parsers__admin_target__native_target__invalid_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/top"), server::error::invalid_target);
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/configuration"), server::error::invalid_target);
    BOOST_REQUIRE_EQUAL(admin_target(out, "/v3/block/height/123"), server::error::invalid_target);
}

BOOST_AUTO_TEST_SUITE_END()

