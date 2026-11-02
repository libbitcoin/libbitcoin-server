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
#include <future>

using namespace system;
namespace http = network::http;
namespace ws = network::ws;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

esplora_setup_fixture::esplora_setup_fixture(const initializer& setup,
    const configurator& configure, bool start)
  : config_
    {
        system::chain::selection::mainnet,
        test::web_pages,
        test::web_pages
    },
    store_
    {
        [&]() NOEXCEPT -> const database::settings&
        {
            test::configure(config_);
            auto& esplora = config_.server.esplora;
            esplora.binds = { { ESPLORA_ENDPOINT } };
            esplora.connections = 1;
            esplora.inactivity_minutes = 1;
            config_.node.delay_inbound = false;
            config_.node.minimum_fee_rate = 0.0;
            config_.network.enable_relay = true;
            config_.network.inbound.connections = 0;
            config_.network.outbound.connections = 0;
            config_.database.path = TEST_DIRECTORY;
            if (configure)
                configure(config_);

            return config_.database;
        }()
    },
    query_{ store_ }, log_{},
    server_{ query_, config_, log_ }
{
    try
    {
        test::clear(test::directory);
        config_.database.interval_depth = 2;
        const auto& esplora = config_.server.esplora;

        // Create and populate the store.
        auto ec = store_.create([](auto, auto) {});
        BC_REQUIRE_MESSAGE(!ec, ec.message());

        // Settings files are read and derived values packed, as by the executor.
        ec = config_.initialize();
        BC_REQUIRE_MESSAGE(!ec, ec.message());

        BC_REQUIRE(setup(query_));

        // The node (chasers and address pool), bypassed by default.
        if (start)
        {
            std::promise<code> started{};
            server_.start([&](const code& ec) NOEXCEPT
            {
                started.set_value(ec);
            });

            ec = started.get_future().get();
            BC_REQUIRE_MESSAGE(!ec, ec.message());
        }

        // Run the server.
        std::promise<code> running{};
        server_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        // Block until server is running.
        ec = running.get_future().get();
        BC_REQUIRE_MESSAGE(!ec, ec.message());
        REQUIRE_SUCCESS(socket_, connect, esplora.binds.back().to_endpoint());
    }
    catch (...)
    {
        teardown();
        throw;
    }
}

esplora_setup_fixture::~esplora_setup_fixture()
{
    network::code ec{};
    if (websocket_.has_value())
    {
        ec = websocket_.value().close(network::ws::close_code::normal);

        // Expected and harmless during fixture teardown:
        // websocket_closed   : normal (graceful handshake).
        // peer_disconnect    : dropped by the server (invalid request test).
        if (ec &&
            ec != network::error::websocket_closed &&
            ec != network::error::peer_disconnect)
        {
            BC_WARN_MESSAGE(false, ec.message());
        }
    }
    else
    {
        socket_.close();
    }

    teardown();
}

void esplora_setup_fixture::teardown()
{
    server_.close();
    const auto ec = store_.close([](auto, auto){});
    BC_WARN_MESSAGE(!ec, ec.message());
    test::clear(test::directory);
}

esplora_setup_fixture::string_request
esplora_setup_fixture::create_request(std::string_view target)
{
    string_request request{ http::verb::get, target, network::http::version_1_1 };
    request.set(http::field::host, "localhost");
    request.set(http::field::user_agent, "libbitcoin");
    request.keep_alive(true);
    return request;
}

http::status esplora_setup_fixture::get_status(std::string_view target)
{
    test::write_http(socket_, create_request(target));

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

http::status esplora_setup_fixture::get_status(std::string_view target,
    http::field name, std::string_view value)
{
    auto request = create_request(target);
    request.set(name, value);
    test::write_http(socket_, request);

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

std::string esplora_setup_fixture::get_text(std::string_view target)
{
    test::write_http(socket_, create_request(target));

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());
    BC_CHECK_EQUAL(response.result(), http::status::ok);

    return response.body();
}

system::data_chunk esplora_setup_fixture::get_data(std::string_view target)
{
    test::write_http(socket_, create_request(target));

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::chunk_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());
    BC_CHECK_EQUAL(response.result(), http::status::ok);

    return system::data_chunk(response.body().cbegin(), response.body().cend());
}

// The network json body does not support reading a document consisting
// of only a top-level primitive, because it supports streaming and non-
// streaming. So instead just use a string buffer and parse explicitly.
system::json::value esplora_setup_fixture::get_json(std::string_view target)
{
    test::write_http(socket_, create_request(target));

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());
    BC_CHECK_EQUAL(response.result(), http::status::ok);

    return test::parse_json(response.body());
}

esplora_setup_fixture::string_request
esplora_setup_fixture::create_post(std::string_view target,
    std::string_view body)
{
    string_request request{ http::verb::post, target, network::http::version_1_1 };
    request.set(http::field::host, "localhost");
    request.set(http::field::user_agent, "libbitcoin");
    request.set(http::field::content_type, "text/plain");
    request.body() = std::string{ body };
    request.prepare_payload();
    request.keep_alive(true);
    return request;
}

http::status esplora_setup_fixture::post_status(std::string_view target,
    std::string_view body)
{
    test::write_http(socket_, create_post(target, body));

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

http::status esplora_setup_fixture::post_status(std::string_view target,
    std::string_view body, http::field name, std::string_view value)
{
    auto request = create_post(target, body);
    request.set(name, value);
    test::write_http(socket_, request);

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

std::string esplora_setup_fixture::post_text(std::string_view target,
    std::string_view body)
{
    test::write_http(socket_, create_post(target, body));

    http::flat_buffer buffer{};
    network::code ec{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.body();
}

network::code esplora_setup_fixture::ws_upgrade()
{
    network::code ec{};
    BC_CHECK(!websocket_.has_value());

    websocket_.emplace(socket_);
    ec = websocket_.value().handshake("localhost", "/");
    return ec;
}

data_chunk esplora_setup_fixture::ws_receive()
{
    http::flat_buffer buffer{};
    network::code ec{};
    BC_CHECK(websocket_.has_value());

    ec = websocket_.value().read(buffer);
    BC_CHECK_MESSAGE(!ec, ec.message());

    const auto data = pointer_cast<uint8_t>(buffer.data().data());
    return { data, std::next(data, buffer.data().size()) };
}

bool esplora_setup_fixture::ws_dropped(std::string_view message)
{
    network::code ec{};
    BC_CHECK(websocket_.has_value());

    ec = websocket_.value().write(message);
    BC_CHECK_MESSAGE(!ec, ec.message());

    http::flat_buffer buffer{};
    ec = websocket_.value().read(buffer);
    return ec == network::error::websocket_closed;
}

std::string esplora_setup_fixture::ws_get_text(std::string_view message)
{
    network::code ec{};
    BC_CHECK(websocket_.has_value());

    ec = websocket_.value().write(message);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return to_string(ws_receive());
}

system::json::value esplora_setup_fixture::ws_get_json(std::string_view message)
{
    return test::parse_json(ws_get_text(message));
}

BC_POP_WARNING()
