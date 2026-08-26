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

BOOST_AUTO_TEST_SUITE(bitcoind_query_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;

constexpr auto expected_hash = base16_hash(
    "0000000000000000000000000000000000000000000000000000000000000042");
static const std::string test_hash = encode_hash(expected_hash);

static const object_t& params_of(const request_t& request) NOEXCEPT
{
    BOOST_REQUIRE(request.params.has_value());
    BOOST_REQUIRE(std::holds_alternative<object_t>(request.params.value()));
    return std::get<object_t>(request.params.value());
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_query__count__overlays_target_default)
{
    request_t out{};
    const auto target = "/rest/headers/" + test_hash + ".json?count=7";
    BOOST_REQUIRE(!bitcoind_target(out, target));
    BOOST_REQUIRE(bitcoind_query(out, target));
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(params_of(out).at("count").value()), 7u);
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_query__no_query__target_default)
{
    request_t out{};
    const auto target = "/rest/headers/" + test_hash + ".json";
    BOOST_REQUIRE(!bitcoind_target(out, target));
    BOOST_REQUIRE(bitcoind_query(out, target));
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(params_of(out).at("count").value()), 5u);
}

BOOST_AUTO_TEST_CASE(parsers__bitcoind_query__malformed_count__false)
{
    request_t out{};
    const auto target = "/rest/headers/" + test_hash + ".json?count=abc";
    BOOST_REQUIRE(!bitcoind_target(out, target));
    BOOST_REQUIRE(!bitcoind_query(out, target));
}

BOOST_AUTO_TEST_SUITE_END()
