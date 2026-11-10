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
#include "../../mocks/blocks.hpp"
#include "esplora_setup_fixture.hpp"

using namespace system;
namespace http = network::http;
namespace ws = network::ws;

BC_FIXTURE_TEST_SUITE(esplora_tests, esplora_ten_block_setup_fixture)

// upgrade
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__ws_upgrade__always__success)
{
    const auto ec = ws_upgrade();
    BC_REQUIRE_MESSAGE(!ec, ec.message());
}

// targets
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__ws_tip_height__text__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_text("/blocks/tip/height"), "9");
}

BC_AUTO_TEST_CASE(esplora__ws_tip_hash__text__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_text("/blocks/tip/hash"), encode_hash(test::block9.hash()));
}

BC_AUTO_TEST_CASE(esplora__ws_block__json__expected)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/block/" + encode_hash(test::block1.hash()));
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("height").as_int64(), 1);
}

BC_AUTO_TEST_CASE(esplora__ws_mempool__json__empty)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/mempool/txids");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

BC_AUTO_TEST_CASE(esplora__ws_pipelined__ordered__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_text("/block-height/0"), encode_hash(test::genesis.hash()));
    BC_REQUIRE_EQUAL(ws_get_text("/block-height/1"), encode_hash(test::block1.hash()));
}

// body frame
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__ws_broadcast__delimited_body__rejected)
{
    BC_REQUIRE(!ws_upgrade());

    const auto tx0 = encode_base16(test::genesis.transactions_ptr()->front()->to_data(true));
    BC_REQUIRE(!ws_get_text("/tx\n" + tx0).empty());
}

BC_AUTO_TEST_CASE(esplora__ws_broadcast__missing_body__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/tx").starts_with("Bad Request"));
}

BC_AUTO_TEST_CASE(esplora__ws_invalid_target__not_found)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/bogus").starts_with("Not Found"));
}

BC_AUTO_TEST_SUITE_END()
