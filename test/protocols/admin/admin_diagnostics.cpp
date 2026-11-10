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
#include "admin_setup_fixture.hpp"

using namespace network::http;

BC_FIXTURE_TEST_SUITE(admin_tests, admin_ten_block_setup_fixture)

// log subscribe (http)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(admin__log_subscribe__json__previous_zero)
{
    const auto response = get_json("/v1/log/subscribe?filter=1&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__omitted_filter__previous_zero)
{
    const auto response = get_json("/v1/log/subscribe?format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__repeat__previous_expected)
{
    auto response = get_json("/v1/log/subscribe?filter=3&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);

    response = get_json("/v1/log/subscribe?filter=5&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 3);

    response = get_json("/v1/log/subscribe?filter=0&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 5);

    response = get_json("/v1/log/subscribe?filter=0&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__maximum_filter__previous_zero)
{
    // 2^53 - 1 is the maximum exact json number.
    const auto response = get_json("/v1/log/subscribe?filter=9007199254740991&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__excess_filter__bad_request)
{
    // 2^53 exceeds the maximum exact json number.
    const auto value = get_status("/v1/log/subscribe?filter=9007199254740992&format=json");
    BC_REQUIRE(value == status::bad_request);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__non_numeric_filter__bad_request)
{
    const auto value = get_status("/v1/log/subscribe?filter=abc&format=json");
    BC_REQUIRE(value == status::bad_request);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__leading_zero_filter__bad_request)
{
    const auto value = get_status("/v1/log/subscribe?filter=01&format=json");
    BC_REQUIRE(value == status::bad_request);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__xml__bad_request)
{
    // An unrecognized format value is malformed, not a negotiation failure.
    const auto value = get_status("/v1/log/subscribe?filter=1&format=xml");
    BC_REQUIRE(value == status::bad_request);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__no_format__not_acceptable)
{
    const auto value = get_status("/v1/log/subscribe?filter=1");
    BC_REQUIRE(value == status::not_acceptable);
}

// event subscribe (http)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(admin__event_subscribe__json__previous_zero)
{
    const auto response = get_json("/v1/event/subscribe?filter=1&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BC_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

// dispatch (http)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(admin__dispatch__invalid_target__bad_request)
{
    BC_REQUIRE(get_status("/v1/bogus") == status::bad_request);
}

BC_AUTO_TEST_CASE(admin__dispatch__html__bad_request)
{
    BC_REQUIRE(get_status("/v1/log/subscribe?filter=1&format=html") == status::bad_request);
}

// subscribe (websockets)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(admin__ws_log_subscribe__json__previous_zero)
{
    BC_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);
}

BC_AUTO_TEST_CASE(admin__ws_subscribe__channels__independent)
{
    BC_REQUIRE(!ws_upgrade());

    // Masks use bits above defined levels/events (no live traffic).
    auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    ack = ws_get_json("/v1/event/subscribe?filter=2048");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    ack = ws_get_json("/v1/log/subscribe?filter=0");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 1024);

    ack = ws_get_json("/v1/event/subscribe?filter=0");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 2048);
}

BC_AUTO_TEST_CASE(admin__ws_log_subscribe__invalid_filter__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/log/subscribe?filter=xyz").starts_with("Bad Request"));
}

BC_AUTO_TEST_CASE(admin__ws_log_subscribe__html__not_acceptable)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/log/subscribe?filter=1&format=html").starts_with("Not Acceptable"));
}

BC_AUTO_TEST_CASE(admin__ws_dispatch__invalid_target__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/bogus").starts_with("Bad Request"));
}

BC_AUTO_TEST_CASE(admin__ws_dispatch__missing_subcomponent__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/log").starts_with("Bad Request"));
}

// streams (websockets)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(admin__ws_log_subscribe__write__notified)
{
    BC_REQUIRE(!ws_upgrade());

    // Bit 10 is above defined levels; the server's own request logging
    // (generated by this test's traffic) can never match the mask.
    const auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    write(10, "test");

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("level").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("time").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("message").is_string());
    BC_REQUIRE_EQUAL(frame.at("level").as_int64(), 10);
    BC_REQUIRE_EQUAL(frame.at("message").as_string(), "test");
}

BC_AUTO_TEST_CASE(admin__ws_log_subscribe__unfiltered_level__not_notified)
{
    BC_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    // Sequential commitment on the logger strand orders these writes.
    write(11, "dropped");
    write(10, "matched");

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("level").is_int64());
    BC_REQUIRE_EQUAL(frame.at("level").as_int64(), 10);
    REQUIRE_NO_THROW_TRUE(frame.at("message").is_string());
    BC_REQUIRE_EQUAL(frame.at("message").as_string(), "matched");
}

BC_AUTO_TEST_CASE(admin__ws_log_subscribe__live_level__eventually_notified)
{
    BC_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/log/subscribe?filter=1");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    write(0, "sentinel");

    // Server self-logging may interleave; drain to the sentinel.
    while (ws_receive_json().at("message").as_string() != "sentinel");
}

BC_AUTO_TEST_CASE(admin__ws_event_subscribe__fire__notified)
{
    BC_REQUIRE(!ws_upgrade());

    // block_archived (bit 3), no node activity fires events in this test.
    const auto ack = ws_get_json("/v1/event/subscribe?filter=8");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    fire(node::events::block_archived, 42);

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("time").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("event").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("value").is_int64());
    BC_REQUIRE_EQUAL(frame.at("event").as_int64(), node::events::block_archived);
    BC_REQUIRE_EQUAL(frame.at("value").as_int64(), 42);
}

BC_AUTO_TEST_CASE(admin__ws_event_subscribe__unfiltered_event__not_notified)
{
    BC_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/event/subscribe?filter=8");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BC_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    fire(node::events::header_archived, 1);
    fire(node::events::block_archived, 2);

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("event").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("value").is_int64());
    BC_REQUIRE_EQUAL(frame.at("event").as_int64(), node::events::block_archived);
    BC_REQUIRE_EQUAL(frame.at("value").as_int64(), 2);
}

BC_AUTO_TEST_CASE(admin__ws_event_subscribe__unsubscribed__not_notified)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_json("/v1/event/subscribe?filter=8").at("previous").as_int64(), 0);
    BC_REQUIRE_EQUAL(ws_get_json("/v1/event/subscribe?filter=0").at("previous").as_int64(), 8);
    BC_REQUIRE_EQUAL(ws_get_json("/v1/log/subscribe?filter=1024").at("previous").as_int64(), 0);

    fire(node::events::block_archived, 1);
    write(10, "marker");

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("message").is_string());
    BC_REQUIRE_EQUAL(frame.at("message").as_string(), "marker");
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(admin_scoped_credential_tests, admin_scoped_credential_setup_fixture)

BC_AUTO_TEST_CASE(admin__event_subscribe__scoped_credential__ok)
{
    BC_REQUIRE_EQUAL(get_status("/v1/event/subscribe?filter=1&format=json", network::http::field::authorization, ADMIN_TEST_AUTHORIZATION), status::ok);
}

BC_AUTO_TEST_CASE(admin__log_subscribe__scoped_credential_unlisted_method__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/log/subscribe?filter=1&format=json", network::http::field::authorization, ADMIN_TEST_AUTHORIZATION), status::forbidden);
}

BC_AUTO_TEST_CASE(admin__event_subscribe__no_credential__unauthorized)
{
    BC_REQUIRE_EQUAL(get_status("/v1/event/subscribe?filter=1&format=json"), status::unauthorized);
}

BC_AUTO_TEST_SUITE_END()
