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

BC_AUTO_TEST_SUITE(electrum_version_tests)

using namespace electrum;
using number = system::config::version;

// version_to_number

BC_AUTO_TEST_CASE(electrum_version__version_to_number__all__expected)
{
    BC_REQUIRE(version_to_number(version::v0_0)   == number(0, 0));
    BC_REQUIRE(version_to_number(version::v0_6)   == number(0, 6));
    BC_REQUIRE(version_to_number(version::v0_8)   == number(0, 8));
    BC_REQUIRE(version_to_number(version::v0_9)   == number(0, 9));
    BC_REQUIRE(version_to_number(version::v0_10)  == number(0, 10));
    BC_REQUIRE(version_to_number(version::v1_0)   == number(1, 0));
    BC_REQUIRE(version_to_number(version::v1_1)   == number(1, 1));
    BC_REQUIRE(version_to_number(version::v1_2)   == number(1, 2));
    BC_REQUIRE(version_to_number(version::v1_3)   == number(1, 3));
    BC_REQUIRE(version_to_number(version::v1_4)   == number(1, 4));
    BC_REQUIRE(version_to_number(version::v1_4_1) == number(1, 4, 1));
    BC_REQUIRE(version_to_number(version::v1_4_2) == number(1, 4, 2));
    BC_REQUIRE(version_to_number(version::v1_6)   == number(1, 6));
    BC_REQUIRE(version_to_number(version::v1_7)   == number(1, 7));
}

// version_to_string

BC_AUTO_TEST_CASE(electrum_version__version_to_string__all__expected)
{
    BC_REQUIRE_EQUAL(version_to_string(version::v0_0), "0.0");
    BC_REQUIRE_EQUAL(version_to_string(version::v0_6), "0.6");
    BC_REQUIRE_EQUAL(version_to_string(version::v0_8), "0.8");
    BC_REQUIRE_EQUAL(version_to_string(version::v0_9), "0.9");
    BC_REQUIRE_EQUAL(version_to_string(version::v0_10), "0.10");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_0), "1.0");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_1), "1.1");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_2), "1.2");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_3), "1.3");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_4), "1.4");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_4_1), "1.4.1");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_4_2), "1.4.2");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_6), "1.6");
    BC_REQUIRE_EQUAL(version_to_string(version::v1_7), "1.7");
}

// version_from_string

BC_AUTO_TEST_CASE(electrum_version__version_from_string__defined__expected)
{
    number out{};
    BC_REQUIRE(version_from_string(out, "0.0"));
    BC_REQUIRE(out == number(0, 0));
    BC_REQUIRE(version_from_string(out, "0.10"));
    BC_REQUIRE(out == number(0, 10));
    BC_REQUIRE(version_from_string(out, "1.4.2"));
    BC_REQUIRE(out == number(1, 4, 2));
    BC_REQUIRE(version_from_string(out, "1.7"));
    BC_REQUIRE(out == number(1, 7));
}

BC_AUTO_TEST_CASE(electrum_version__version_from_string__undefined__expected)
{
    number out{};
    BC_REQUIRE(version_from_string(out, "0.7"));
    BC_REQUIRE(out == number(0, 7));
    BC_REQUIRE(version_from_string(out, "1.5"));
    BC_REQUIRE(out == number(1, 5));
    BC_REQUIRE(version_from_string(out, "1.8"));
    BC_REQUIRE(out == number(1, 8));
    BC_REQUIRE(version_from_string(out, "2.0"));
    BC_REQUIRE(out == number(2, 0));
    BC_REQUIRE(version_from_string(out, "42.24.7.11"));
    BC_REQUIRE(out == number(42, 24, 7, 11));
}

BC_AUTO_TEST_CASE(electrum_version__version_from_string__malformed__false)
{
    number out{};
    BC_REQUIRE(!version_from_string(out, ""));
    BC_REQUIRE(!version_from_string(out, "42"));
    BC_REQUIRE(!version_from_string(out, "1."));
    BC_REQUIRE(!version_from_string(out, "1..4"));
    BC_REQUIRE(!version_from_string(out, "1.x"));
    BC_REQUIRE(!version_from_string(out, "1.4x"));
    BC_REQUIRE(!version_from_string(out, "1.4.2.0.0"));
    BC_REQUIRE(!version_from_string(out, "1.-4"));
    BC_REQUIRE(!version_from_string(out, "1.4 junk"));
    BC_REQUIRE(!version_from_string(out, " 1.4"));
}

// version_floor

BC_AUTO_TEST_CASE(electrum_version__version_floor__defined__same)
{
    BC_REQUIRE(version_floor(number(0, 0)) == version::v0_0);
    BC_REQUIRE(version_floor(number(0, 6)) == version::v0_6);
    BC_REQUIRE(version_floor(number(1, 0)) == version::v1_0);
    BC_REQUIRE(version_floor(number(1, 4, 1)) == version::v1_4_1);
    BC_REQUIRE(version_floor(number(1, 4, 2)) == version::v1_4_2);
    BC_REQUIRE(version_floor(number(1, 7)) == version::v1_7);
}

BC_AUTO_TEST_CASE(electrum_version__version_floor__undefined__next_lower)
{
    BC_REQUIRE(version_floor(number(0, 5)) == version::v0_0);
    BC_REQUIRE(version_floor(number(0, 7)) == version::v0_6);
    BC_REQUIRE(version_floor(number(1, 4, 3)) == version::v1_4_2);
    BC_REQUIRE(version_floor(number(1, 5)) == version::v1_4_2);
    BC_REQUIRE(version_floor(number(1, 8)) == version::v1_7);
    BC_REQUIRE(version_floor(number(2, 0)) == version::v1_7);
}

BC_AUTO_TEST_SUITE_END()

BC_AUTO_TEST_SUITE(electrum_request_tests)

using namespace network::rpc;

BC_AUTO_TEST_CASE(electrum_request__server_version__single_value__minimum_appended)
{
    request_t message{ .method = "server.version", .params = params_t{ value_t{ string_t{ "foobar" } } } };
    electrum_request(message, electrum::version::v0_0, { 1, 0 }, { 1, 7 });
    BC_REQUIRE(message.params.has_value());
    BC_REQUIRE(std::holds_alternative<array_t>(message.params.value()));

    const auto& params = std::get<array_t>(message.params.value());
    BC_REQUIRE_EQUAL(params.size(), 2u);
    BC_REQUIRE(std::holds_alternative<string_t>(params.at(0).value()));
    BC_REQUIRE(std::holds_alternative<string_t>(params.at(1).value()));
    BC_REQUIRE_EQUAL(std::get<string_t>(params.at(0).value()), "foobar");
    BC_REQUIRE_EQUAL(std::get<string_t>(params.at(1).value()), "1.0");
}

BC_AUTO_TEST_CASE(electrum_request__transaction_get__unset_version__verbose_retained)
{
    request_t message{ .method = "blockchain.transaction.get", .params = params_t{ array_t{ value_t{ string_t{ "foobar" } }, value_t{ true } } } };
    electrum_request(message, electrum::version::v0_0, { 1, 0 }, { 1, 7 });
    BC_REQUIRE(message.params.has_value());
    BC_REQUIRE(std::holds_alternative<array_t>(message.params.value()));
    BC_REQUIRE_EQUAL(std::get<array_t>(message.params.value()).size(), 2u);
}

BC_AUTO_TEST_CASE(electrum_request__transaction_get__version_1_0__height_dropped)
{
    request_t message{ .method = "blockchain.transaction.get", .params = params_t{ array_t{ value_t{ string_t{ "foobar" } }, value_t{ true } } } };
    electrum_request(message, electrum::version::v1_0, { 1, 0 }, { 1, 7 });
    BC_REQUIRE(message.params.has_value());
    BC_REQUIRE(std::holds_alternative<array_t>(message.params.value()));
    BC_REQUIRE_EQUAL(std::get<array_t>(message.params.value()).size(), 1u);
}

BC_AUTO_TEST_SUITE_END()
