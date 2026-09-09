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
#include "electrum_setup_fixture.hpp"

using namespace system;

// Electrum is served over one channel on three transports: raw tcp (by
// downgrade), http POST, and websocket (by upgrade). The method suites cover
// tcp exhaustively; these verify that the other two reach the same protocol
// and differ only where the transport requires it (notification push).

BOOST_FIXTURE_TEST_SUITE(electrum_transport_tests, electrum_ten_block_setup_fixture)

// http POST
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(electrum__post__handshake__negotiated)
{
    BOOST_REQUIRE(post_handshake(electrum::version::v1_4));
}

BOOST_AUTO_TEST_CASE(electrum__post__numblocks_subscribe__returns_9)
{
    BOOST_REQUIRE(post_handshake(electrum::version::v1_0));

    const auto response = post(R"({"id":700,"method":"blockchain.numblocks.subscribe","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 700);
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(electrum__post__unknown_method__method_not_found)
{
    BOOST_REQUIRE(post_handshake(electrum::version::v1_4));

    const auto response = post(R"({"id":701,"method":"server.bogus","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(),
        server::error::electrum::method_not_found);
}

// Notifications cannot be pushed over POST, so the subscription is accepted
// and held (not dropped), and the events are simply not computed.
BOOST_AUTO_TEST_CASE(electrum__post__subscribed_event__no_notification)
{
    BOOST_REQUIRE(post_handshake(electrum::version::v1_0));

    const auto response = post(R"({"id":702,"method":"blockchain.numblocks.subscribe","params":[]})");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);

    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chase::organized, { 10_u32 });

    // The channel remains usable and answers the next request (not a
    // notification), so nothing was pushed onto the http connection.
    const auto next = post(R"({"id":703,"method":"blockchain.relayfee","params":[]})");
    BOOST_REQUIRE_EQUAL(next.at("id").as_int64(), 703);
    REQUIRE_NO_THROW_TRUE(next.at("result").is_double());
}

BOOST_AUTO_TEST_SUITE_END()

// websocket
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(electrum_websocket_tests, electrum_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(electrum__ws__handshake__negotiated)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_handshake(electrum::version::v1_4));
}

BOOST_AUTO_TEST_CASE(electrum__ws__numblocks_subscribe__returns_9)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_handshake(electrum::version::v1_0));

    const auto response = ws_get(R"({"id":800,"method":"blockchain.numblocks.subscribe","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 800);
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(electrum__ws__unknown_method__method_not_found)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_handshake(electrum::version::v1_4));

    const auto response = ws_get(R"({"id":801,"method":"server.bogus","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(),
        server::error::electrum::method_not_found);
}

// The notification gate is websocket() || downgraded(), so ws pushes.
BOOST_AUTO_TEST_CASE(electrum__ws__subscribed_event__notified)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_handshake(electrum::version::v1_0));

    const auto response = ws_get(R"({"id":802,"method":"blockchain.numblocks.subscribe","params":[]})");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);

    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chase::organized, { 10_u32 });

    const auto notification = ws_receive();
    REQUIRE_NO_THROW_TRUE(notification.at("method").is_string());
    BOOST_REQUIRE_EQUAL(notification.at("method").as_string(),
        "blockchain.numblocks.subscribe");
    BOOST_CHECK_EQUAL(notification.at("params").as_int64(), 10);
}

BOOST_AUTO_TEST_SUITE_END()
