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
#include "../../test.hpp"
#include "electrum.hpp"

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

// blockchain.headers.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__default__expected)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":80,"method":"blockchain.headers.subscribe","params":[]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("height").as_int64(), 9);
    BOOST_CHECK_EQUAL(result.at("hex").as_string(), system::encode_base16(header9_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__jsonrpc_2__expected)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"jsonrpc":"2.0","id":81,"method":"blockchain.headers.subscribe"})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("height").as_int64(), 9);
    BOOST_CHECK_EQUAL(result.at("hex").as_string(), system::encode_base16(header9_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__id_preserved__expected)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":123,"method":"blockchain.headers.subscribe","params":[]})" "\n");
    BOOST_CHECK_EQUAL(response.at("id").as_int64(), 123);
    BOOST_CHECK(response.at("result").is_object());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__empty_params__expected)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":82,"method":"blockchain.headers.subscribe","params":[]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("height").as_int64(), 9);
    BOOST_CHECK_EQUAL(result.at("hex").as_string(), system::encode_base16(header9_data));
}

BOOST_AUTO_TEST_SUITE_END()