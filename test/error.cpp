/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include "test.hpp"

BOOST_AUTO_TEST_SUITE(error_tests)

// error_t
// These test std::error_code equality operator overrides.

// general

BOOST_AUTO_TEST_CASE(error_t__code__success__false_expected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "success");
}

// server (url parse codes)

BOOST_AUTO_TEST_CASE(error_t__code__empty_path__true_expected_message)
{
    constexpr auto value = error::empty_path;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "empty_path");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_number__true_expected_message)
{
    constexpr auto value = error::invalid_number;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_number");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_hash__true_expected_message)
{
    constexpr auto value = error::invalid_hash;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_hash");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_version__true_expected_message)
{
    constexpr auto value = error::missing_version;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_version");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_target__true_expected_message)
{
    constexpr auto value = error::missing_target;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_target");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_target__true_expected_message)
{
    constexpr auto value = error::invalid_target;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_target");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_hash__true_expected_message)
{
    constexpr auto value = error::missing_hash;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_hash");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_height__true_expected_message)
{
    constexpr auto value = error::missing_height;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_height");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_position__true_expected_message)
{
    constexpr auto value = error::missing_position;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_position");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_id_type__true_expected_message)
{
    constexpr auto value = error::missing_id_type;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_id_type");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_id_type__true_expected_message)
{
    constexpr auto value = error::invalid_id_type;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_id_type");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_type_id__true_expected_message)
{
    constexpr auto value = error::missing_type_id;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_type_id");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_component__true_expected_message)
{
    constexpr auto value = error::missing_component;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_component");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_component__true_expected_message)
{
    constexpr auto value = error::invalid_component;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_component");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_subcomponent__true_expected_message)
{
    constexpr auto value = error::invalid_subcomponent;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_subcomponent");
}

BOOST_AUTO_TEST_CASE(error_t__code__extra_segment__true_expected_message)
{
    constexpr auto value = error::extra_segment;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "extra_segment");
}

// server (rpc response codes)

BOOST_AUTO_TEST_CASE(error_t__code__not_found__true_expected_message)
{
    constexpr auto value = error::not_found;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "not_found");
}

BOOST_AUTO_TEST_CASE(error_t__code__not_implemented__true_expected_message)
{
    constexpr auto value = error::not_implemented;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "not_implemented");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_argument__true_expected_message)
{
    constexpr auto value = error::invalid_argument;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_argument");
}

BOOST_AUTO_TEST_CASE(error_t__code__argument_overflow__true_expected_message)
{
    constexpr auto value = error::argument_overflow;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "argument_overflow");
}

BOOST_AUTO_TEST_CASE(error_t__code__target_overflow__true_expected_message)
{
    constexpr auto value = error::target_overflow;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "target_overflow");
}

BOOST_AUTO_TEST_CASE(error_t__code__server_error__true_expected_message)
{
    constexpr auto value = error::server_error;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "server_error");
}

BOOST_AUTO_TEST_SUITE_END()
