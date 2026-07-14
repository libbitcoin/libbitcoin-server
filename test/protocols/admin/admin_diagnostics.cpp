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
#include "admin_setup_fixture.hpp"

using namespace network::http;

BOOST_FIXTURE_TEST_SUITE(admin_tests, admin_ten_block_setup_fixture)

// log subscribe (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(admin__log_subscribe__json__previous_zero)
{
    const auto response = get_json("/v1/log/subscribe?filter=1&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__omitted_filter__previous_zero)
{
    const auto response = get_json("/v1/log/subscribe?format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__repeat__previous_expected)
{
    auto response = get_json("/v1/log/subscribe?filter=3&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);

    response = get_json("/v1/log/subscribe?filter=5&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 3);

    response = get_json("/v1/log/subscribe?filter=0&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 5);

    response = get_json("/v1/log/subscribe?filter=0&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__maximum_filter__previous_zero)
{
    // 2^53 - 1 is the maximum exact json number.
    const auto response = get_json("/v1/log/subscribe?filter=9007199254740991&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__excess_filter__bad_request)
{
    // 2^53 exceeds the maximum exact json number.
    const auto value = get_status("/v1/log/subscribe?filter=9007199254740992&format=json");
    BOOST_REQUIRE(value == status::bad_request);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__non_numeric_filter__bad_request)
{
    const auto value = get_status("/v1/log/subscribe?filter=abc&format=json");
    BOOST_REQUIRE(value == status::bad_request);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__leading_zero_filter__bad_request)
{
    const auto value = get_status("/v1/log/subscribe?filter=01&format=json");
    BOOST_REQUIRE(value == status::bad_request);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__xml__bad_request)
{
    // An unrecognized format value is malformed, not a negotiation failure.
    const auto value = get_status("/v1/log/subscribe?filter=1&format=xml");
    BOOST_REQUIRE(value == status::bad_request);
}

BOOST_AUTO_TEST_CASE(admin__log_subscribe__no_format__not_acceptable)
{
    const auto value = get_status("/v1/log/subscribe?filter=1");
    BOOST_REQUIRE(value == status::not_acceptable);
}

// event subscribe (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(admin__event_subscribe__json__previous_zero)
{
    const auto response = get_json("/v1/event/subscribe?filter=1&format=json");
    REQUIRE_NO_THROW_TRUE(response.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("previous").as_int64(), 0);
}

// subscribe (websockets)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(admin__ws_log_subscribe__json__previous_zero)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(admin__ws_subscribe__channels__independent)
{
    BOOST_REQUIRE(!ws_upgrade());

    // Masks use bits above defined levels/events (no live traffic).
    auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    ack = ws_get_json("/v1/event/subscribe?filter=2048");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    ack = ws_get_json("/v1/log/subscribe?filter=0");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 1024);

    ack = ws_get_json("/v1/event/subscribe?filter=0");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 2048);
}

BOOST_AUTO_TEST_CASE(admin__ws_log_subscribe__invalid_filter__error_eof)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_dropped("/v1/log/subscribe?filter=xyz"));
}

BOOST_AUTO_TEST_CASE(admin__ws_log_subscribe__html__error_eof)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_dropped("/v1/log/subscribe?filter=1&format=html"));
}

// streams (websockets)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(admin__ws_log_subscribe__write__notified)
{
    BOOST_REQUIRE(!ws_upgrade());

    // Bit 10 is above defined levels; the server's own request logging
    // (generated by this test's traffic) can never match the mask.
    const auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    write(10, "test");

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("level").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("time").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("message").is_string());
    BOOST_REQUIRE_EQUAL(frame.at("level").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(frame.at("message").as_string(), "test");
}

BOOST_AUTO_TEST_CASE(admin__ws_log_subscribe__unfiltered_level__not_notified)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/log/subscribe?filter=1024");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    // Sequential commitment on the logger strand orders these writes.
    write(11, "dropped");
    write(10, "matched");

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("level").is_int64());
    BOOST_REQUIRE_EQUAL(frame.at("level").as_int64(), 10);
    REQUIRE_NO_THROW_TRUE(frame.at("message").is_string());
    BOOST_REQUIRE_EQUAL(frame.at("message").as_string(), "matched");
}

BOOST_AUTO_TEST_CASE(admin__ws_log_subscribe__live_level__eventually_notified)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/log/subscribe?filter=1");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    write(0, "sentinel");

    // Server self-logging may interleave; drain to the sentinel.
    while (true)
        if (ws_receive_json().at("message").as_string() == "sentinel")
            break;
}

BOOST_AUTO_TEST_CASE(admin__ws_event_subscribe__fire__notified)
{
    BOOST_REQUIRE(!ws_upgrade());

    // block_archived (bit 3), no node activity fires events in this test.
    const auto ack = ws_get_json("/v1/event/subscribe?filter=8");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    fire(node::events::block_archived, 42);

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("time").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("event").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("value").is_int64());
    BOOST_REQUIRE_EQUAL(frame.at("event").as_int64(), node::events::block_archived);
    BOOST_REQUIRE_EQUAL(frame.at("value").as_int64(), 42);
}

BOOST_AUTO_TEST_CASE(admin__ws_event_subscribe__unfiltered_event__not_notified)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto ack = ws_get_json("/v1/event/subscribe?filter=8");
    REQUIRE_NO_THROW_TRUE(ack.at("previous").is_int64());
    BOOST_REQUIRE_EQUAL(ack.at("previous").as_int64(), 0);

    fire(node::events::header_archived, 1);
    fire(node::events::block_archived, 2);

    const auto frame = ws_receive_json();
    REQUIRE_NO_THROW_TRUE(frame.at("event").is_int64());
    REQUIRE_NO_THROW_TRUE(frame.at("value").is_int64());
    BOOST_REQUIRE_EQUAL(frame.at("event").as_int64(), node::events::block_archived);
    BOOST_REQUIRE_EQUAL(frame.at("value").as_int64(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
