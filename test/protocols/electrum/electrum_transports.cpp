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

// The method suites cover tcp (downgrade); these cover http and ws, which
// differ only in framing and in the push of notifications.

static const code not_implemented{ server::error::electrum::method_not_found };
static const code bad_request{ server::error::electrum::bad_request };

// http POST

BC_FIXTURE_TEST_SUITE(electrum_transport_tests, electrum_ten_block_setup_fixture)

BC_AUTO_TEST_CASE(electrum__post__handshake__negotiated)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_4));
}

BC_AUTO_TEST_CASE(electrum__post__origin__access_control)
{
    const auto response = post_response(R"({"id":707,"method":"server.version","params":["test","1.4"]})", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), rpc_client::status::ok);
    BC_REQUIRE_EQUAL(response[network::http::field::access_control_allow_origin], "http://example.com");
    BC_REQUIRE_EQUAL(response[network::http::field::vary], "origin");
}

BC_AUTO_TEST_CASE(electrum__post__handshaken_origin__access_control)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_0));

    const auto response = post_response(R"({"id":708,"method":"blockchain.numblocks.subscribe","params":[]})", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), rpc_client::status::ok);
    BC_REQUIRE_EQUAL(response[network::http::field::access_control_allow_origin], "http://example.com");
}

BC_AUTO_TEST_CASE(electrum__post__no_origin__no_access_control)
{
    const auto response = post_response(R"({"id":709,"method":"server.version","params":["test","1.4"]})");
    BC_REQUIRE_EQUAL(response.result(), rpc_client::status::ok);
    BC_REQUIRE(is_zero(response.count(network::http::field::access_control_allow_origin)));
    BC_REQUIRE(is_zero(response.count(network::http::field::vary)));
}

BC_AUTO_TEST_CASE(electrum__post__http_1_0__version_1_0)
{
    const auto response = post_response(R"({"id":710,"method":"server.version","params":["test","1.4"]})", {}, network::http::version_1_0);
    BC_REQUIRE_EQUAL(response.result(), rpc_client::status::ok);
    BC_REQUIRE_EQUAL(response.version(), network::http::version_1_0);
}

BC_AUTO_TEST_CASE(electrum__post__numblocks_subscribe__returns_9)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_0));

    const auto response = post(R"({"id":700,"method":"blockchain.numblocks.subscribe","params":[]})");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 700);
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BC_AUTO_TEST_CASE(electrum__post__unknown_method__method_not_found)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_4));

    const auto response = post(R"({"id":701,"method":"server.bogus","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

// POST cannot push, so the subscription is held and no event is sent.
BC_AUTO_TEST_CASE(electrum__post__subscribed_event__no_notification)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_0));

    const auto response = post(R"({"id":702,"method":"blockchain.numblocks.subscribe","params":[]})");
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chases::organized{ 10 });

    // The next response (not a notification) proves nothing was pushed.
    const auto next = post(R"({"id":703,"method":"blockchain.relayfee","params":[]})");
    BC_REQUIRE_EQUAL(next.at("id").as_int64(), 703);
    REQUIRE_NO_THROW_TRUE(next.at("result").is_double());
}

BC_AUTO_TEST_CASE(electrum__post__repeat_version_from_1_4__bad_request)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_4));

    const auto response = post(R"({"id":704,"method":"server.version","params":["foobar","1.4"]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), bad_request.value());
}

BC_AUTO_TEST_CASE(electrum__post__missing_arguments__invalid_params)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_1));

    const auto response = post(R"({"id":705,"method":"blockchain.scripthash.get_balance","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_params);
}

BC_AUTO_TEST_CASE(electrum__post__non_version_opener__answered)
{
    const auto response = post(R"({"id":706,"method":"server.banner","params":[]})");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 706);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
}

BC_AUTO_TEST_CASE(electrum__get__handshake_pending__method_not_allowed)
{
    BC_REQUIRE_EQUAL(get_status("/"), rpc_client::status::method_not_allowed);
}

BC_AUTO_TEST_CASE(electrum__get__after_handshake__method_not_allowed)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_4));
    BC_REQUIRE_EQUAL(get_status("/"), rpc_client::status::method_not_allowed);
}

BC_AUTO_TEST_CASE(electrum__options__after_handshake__ok)
{
    BC_REQUIRE(post_handshake(electrum::version::v1_4));
    BC_REQUIRE_EQUAL(options_status("/"), rpc_client::status::ok);
}

BC_AUTO_TEST_CASE(electrum__tcp__malformed_json__parse_error_null_id)
{
    const auto response = get(R"({"id":1,"method":})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("error"), serialize(response));
    BC_REQUIRE(response.at("id").is_null());
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), network::rpc::reserved::parse_error);
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__tcp__missing_method__invalid_request_with_id)
{
    const auto response = get(R"({"id":5,"params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("error"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 5);
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), network::rpc::reserved::invalid_request);
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__tcp__missing_arguments__continues)
{
    BC_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":6,"method":"blockchain.scripthash.get_balance","params":[]})" "\n");
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), network::rpc::reserved::invalid_params);

    const auto banner = get(R"({"id":7,"method":"server.banner","params":[]})" "\n");
    BC_REQUIRE_EQUAL(banner.at("id").as_int64(), 7);
    REQUIRE_NO_THROW_TRUE(banner.at("result").is_string());
}

BC_AUTO_TEST_CASE(electrum__post__malformed_json__parse_error_null_id)
{
    const auto response = post(R"({"id":1,"method":})");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("error"), serialize(response));
    BC_REQUIRE(response.at("id").is_null());
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), network::rpc::reserved::parse_error);
}

BC_AUTO_TEST_CASE(electrum__post__malformed_json__ok)
{
    BC_REQUIRE_EQUAL(post_status(R"({"id":1,"method":})"), rpc_client::status::ok);
}

BC_AUTO_TEST_SUITE_END()

// scoped credential

BC_FIXTURE_TEST_SUITE(electrum_scoped_credential_tests, electrum_scoped_credential_setup_fixture)

BC_AUTO_TEST_CASE(electrum__post__scoped_credential_unlisted_method__forbidden)
{
    const auto response = post_authorized(R"({"id":900,"method":"server.version","params":["test","1.4"]})", ELECTRUM_TEST_USERNAME, ELECTRUM_TEST_PASSWORD);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto status = post_status_authorized(R"({"id":901,"method":"server.banner","params":[]})", ELECTRUM_TEST_USERNAME, ELECTRUM_TEST_PASSWORD);
    BC_REQUIRE_EQUAL(status, rpc_client::status::forbidden);
}

BC_AUTO_TEST_CASE(electrum__ws__scoped_credential_unlisted_method__unauthorized)
{
    BC_REQUIRE(!ws_upgrade(ELECTRUM_TEST_USERNAME, ELECTRUM_TEST_PASSWORD));
    BC_REQUIRE(ws_handshake(electrum::version::v1_4));

    const auto response = ws_get(R"({"id":902,"method":"server.banner","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), code{ network::error::unauthorized }.value());
    REQUIRE_NO_THROW_TRUE(ws_receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__tcp__credentials_configured__unauthorized)
{
    const auto response = get(R"({"id":903,"method":"server.version","params":["test","1.4"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), code{ network::error::unauthorized }.value());
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_SUITE_END()

// disallowed host

BC_FIXTURE_TEST_SUITE(electrum_hosted_tests, electrum_hosted_setup_fixture)

BC_AUTO_TEST_CASE(electrum__post__disallowed_host__bad_request)
{
    BC_REQUIRE_EQUAL(post_status(R"({"id":904,"method":"server.version","params":["test","1.4"]})"), rpc_client::status::bad_request);
}

BC_AUTO_TEST_SUITE_END()

// websocket

BC_FIXTURE_TEST_SUITE(electrum_websocket_tests, electrum_ten_block_setup_fixture)

BC_AUTO_TEST_CASE(electrum__ws__handshake__negotiated)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_handshake(electrum::version::v1_4));
}

BC_AUTO_TEST_CASE(electrum__ws__non_version_opener__answered)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get(R"({"id":803,"method":"server.banner","params":[]})");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 803);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
}

BC_AUTO_TEST_CASE(electrum__ws__numblocks_subscribe__returns_9)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_handshake(electrum::version::v1_0));

    const auto response = ws_get(R"({"id":800,"method":"blockchain.numblocks.subscribe","params":[]})");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 800);
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BC_AUTO_TEST_CASE(electrum__ws__unknown_method__method_not_found)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_handshake(electrum::version::v1_4));

    const auto response = ws_get(R"({"id":801,"method":"server.bogus","params":[]})");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__ws__subscribed_event__notified)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_handshake(electrum::version::v1_0));

    const auto response = ws_get(R"({"id":802,"method":"blockchain.numblocks.subscribe","params":[]})");
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chases::organized{ 10 });

    const auto notification = ws_receive();
    REQUIRE_NO_THROW_TRUE(notification.at("method").is_string());
    BC_REQUIRE_EQUAL(notification.at("method").as_string(), "blockchain.numblocks.subscribe");
    BC_CHECK_EQUAL(notification.at("params").as_int64(), 10);
}

BC_AUTO_TEST_CASE(electrum__ws__malformed_json__parse_error_null_id)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get(R"({"id":1,"method":})");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("error"), serialize(response));
    BC_REQUIRE(response.at("id").is_null());
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), network::rpc::reserved::parse_error);
    REQUIRE_NO_THROW_TRUE(ws_receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__ws__missing_arguments__continues)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_handshake(electrum::version::v1_4));

    const auto response = ws_get(R"({"id":6,"method":"blockchain.scripthash.get_balance","params":[]})");
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), network::rpc::reserved::invalid_params);

    const auto banner = ws_get(R"({"id":7,"method":"server.banner","params":[]})");
    BC_REQUIRE_EQUAL(banner.at("id").as_int64(), 7);
    REQUIRE_NO_THROW_TRUE(banner.at("result").is_string());
}

BC_AUTO_TEST_SUITE_END()

// server ping
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(electrum_ping_tests, electrum_ping_setup_fixture)

BC_AUTO_TEST_CASE(electrum__ws__ping_interval__server_ping_notified)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_handshake(electrum::version::v1_7));

    const auto notification = ws_receive();
    BC_REQUIRE_EQUAL(notification.at("method").as_string(), "server.ping");
    BC_REQUIRE_EQUAL(notification.at("params").as_array().size(), 1u);
    BC_REQUIRE_EQUAL(notification.at("params").at(0).as_string(), "0000");
}

BC_AUTO_TEST_CASE(electrum__tcp__ping_interval__server_ping_notified)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto notification = receive();
    BC_REQUIRE_EQUAL(notification.at("method").as_string(), "server.ping");
    BC_REQUIRE_EQUAL(notification.at("params").at(0).as_string(), "0000");
}

BC_AUTO_TEST_SUITE_END()