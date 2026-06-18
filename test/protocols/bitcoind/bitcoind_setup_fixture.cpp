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
#include "../../mocks/blocks.hpp"
#include "bitcoind_setup_fixture.hpp"
#include <future>
#include <sstream>

using namespace boost::beast;

namespace {

// Internal linkage to avoid colliding with the native fixture's parse_json.
boost::json::value parse_json(std::string_view value)
{
    try
    {
        return boost::json::parse(value);
    }
    catch (...)
    {
        return {};
    }
}

} // namespace

bitcoind_setup_fixture::bitcoind_setup_fixture(const initializer& setup)
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
            config_.database.path = TEST_DIRECTORY;
            return config_.database;
        }()
    },
    query_{ store_ }, log_{},
    server_{ query_, config_, log_ }
{
    BOOST_REQUIRE_MESSAGE(test::clear(test::directory), "bitcoind setup");

    auto& database_settings = config_.database;
    auto& network_settings = config_.network;
    auto& node_settings = config_.node;
    auto& server_settings = config_.server;
    auto& bitcoind = server_settings.bitcoind;

    bitcoind.binds = { { BITCOIND_ENDPOINT } };
    bitcoind.connections = 1;
    database_settings.interval_depth = 2;
    node_settings.delay_inbound = false;
    node_settings.minimum_fee_rate = 99.0;
    network_settings.inbound.connections = 0;
    network_settings.outbound.connections = 0;

    // Create and populate the store.
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    BOOST_REQUIRE_MESSAGE(setup(query_), "bitcoind initialize");

    // Run the server.
    std::promise<code> running{};
    server_.run([&](const code& ec) NOEXCEPT
    {
        running.set_value(ec);
    });

    // Block until server is running.
    ec = running.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    socket_.connect(bitcoind.binds.back().to_endpoint());
}

bitcoind_setup_fixture::~bitcoind_setup_fixture()
{
    socket_.close();
    server_.close();
    const auto ec = store_.close([](auto, auto){});
    BOOST_WARN_MESSAGE(!ec, ec.message());
    BOOST_WARN_MESSAGE(test::clear(test::directory), "bitcoind cleanup");
}

bitcoind_setup_fixture::string_request
bitcoind_setup_fixture::create_get(std::string_view target)
{
    string_request request{ http::verb::get, target,
        network::http::version_1_1 };
    request.set(http::field::host, "localhost");
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    request.keep_alive(true);
    return request;
}

bitcoind_setup_fixture::string_request
bitcoind_setup_fixture::create_post(std::string_view target,
    std::string_view body)
{
    string_request request{ http::verb::post, target,
        network::http::version_1_1 };
    request.set(http::field::host, "localhost");
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    request.set(http::field::content_type, "application/json");
    request.body() = std::string{ body };
    request.prepare_payload();
    request.keep_alive(true);
    return request;
}

boost::json::value bitcoind_setup_fixture::rpc(std::string_view method,
    std::string_view params)
{
    std::ostringstream body{};
    body << R"({"jsonrpc":"2.0","id":0,"method":")" << method
        << R"(","params":)" << params << "}";
    http::write(socket_, create_post("/", body.str()));

    flat_buffer buffer{};
    network::boost_code ec{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    return parse_json(response.body());
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::rest_status(std::string_view target)
{
    http::write(socket_, create_get(target));

    flat_buffer buffer{};
    network::boost_code ec{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    return response.result();
}

boost::json::value bitcoind_setup_fixture::rest_json(std::string_view target)
{
    http::write(socket_, create_get(target));

    flat_buffer buffer{};
    network::boost_code ec{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK_EQUAL(response.result(), http::status::ok);
    return parse_json(response.body());
}

std::string bitcoind_setup_fixture::rest_text(std::string_view target)
{
    http::write(socket_, create_get(target));

    flat_buffer buffer{};
    network::boost_code ec{};
    http::response<network::http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK_EQUAL(response.result(), http::status::ok);

    auto body = response.body();
    system::trim_right(body);
    return body;
}

system::data_chunk bitcoind_setup_fixture::rest_data(std::string_view target)
{
    http::write(socket_, create_get(target));

    flat_buffer buffer{};
    network::boost_code ec{};
    http::response<network::http::chunk_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK_EQUAL(response.result(), http::status::ok);
    return system::to_chunk(response.body());
}
