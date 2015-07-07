/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-protocol.
 *
 * libbitcoin-protocol is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/server.hpp>

using namespace bc;
using namespace bc::server;
using namespace boost;
using namespace boost::program_options;

BOOST_AUTO_TEST_SUITE(endpont_tests)

BOOST_AUTO_TEST_CASE(endpont__construct__default__expected_empty_values)
{
    endpoint_type endpoint;
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 0u);
}

BOOST_AUTO_TEST_CASE(endpont__construct__host__expected_values)
{
    endpoint_type endpoint("foo.bar");
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "foo.bar");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 0u);
}

BOOST_AUTO_TEST_CASE(endpont__construct__host_port__expected_values)
{
    endpoint_type endpoint("foo.bar:42");
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "foo.bar");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 42u);
}

BOOST_AUTO_TEST_CASE(endpont__construct__scheme_host_port__expected_values)
{
    endpoint_type endpoint("tcp://foo.bar:42");
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "tcp");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "foo.bar");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 42u);
}

BOOST_AUTO_TEST_CASE(endpont__construct__scheme_host__expected_values)
{
    endpoint_type endpoint("tcp://foo.bar");
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "tcp");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "foo.bar");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 0u);
}

// This should throw invalid_option_value.
BOOST_AUTO_TEST_CASE(endpont__construct__no_host__throws_invalid_option_value)
{
    endpoint_type endpoint("tcp://"), invalid_option_value;
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "tcp");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 0u);
}

// This should throw invalid_option_value.
BOOST_AUTO_TEST_CASE(endpont__construct__port_only__throws_invalid_option_value)
{
    endpoint_type endpoint(":42");
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "42");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 0u);
}

// This should throw invalid_option_value.
BOOST_AUTO_TEST_CASE(endpont__construct__single_word_host__throws_invalid_option_value)
{
    endpoint_type endpoint("foobar");
    BOOST_REQUIRE_EQUAL(endpoint.get_scheme(), "");
    BOOST_REQUIRE_EQUAL(endpoint.get_host(), "foobar");
    BOOST_REQUIRE_EQUAL(endpoint.get_port(), 0u);
}

BOOST_AUTO_TEST_SUITE_END()
