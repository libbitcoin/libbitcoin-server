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
using namespace boost::beast;

BOOST_FIXTURE_TEST_SUITE(esplora_tests, esplora_ten_block_setup_fixture)

static const std::string scripthash = encode_hash(null_hash);

// key resolution
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__address__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/address/notanaddress"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__address_txs__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/address/notanaddress/txs"), http::status::bad_request);
}

BOOST_AUTO_TEST_CASE(esplora__address_utxo__undecodable__bad_request)
{
    BOOST_REQUIRE_EQUAL(get_status("/address/notanaddress/utxo"), http::status::bad_request);
}

// address/txs/mempool
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__address_txs_mempool__scripthash__empty)
{
    const auto response = get_json("/scripthash/" + scripthash + "/txs/mempool");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE(response.as_array().empty());
}

// The address index (optional outs head) is not enabled by this fixture.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(esplora__address__index_disabled__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/scripthash/" + scripthash), http::status::not_implemented);
}

BOOST_AUTO_TEST_CASE(esplora__address_txs__index_disabled__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/scripthash/" + scripthash + "/txs"), http::status::not_implemented);
}

BOOST_AUTO_TEST_CASE(esplora__address_utxo__index_disabled__not_implemented)
{
    BOOST_REQUIRE_EQUAL(get_status("/scripthash/" + scripthash + "/utxo"), http::status::not_implemented);
}

BOOST_AUTO_TEST_SUITE_END()
