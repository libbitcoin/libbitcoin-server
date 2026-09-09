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
#include "electrum_setup_fixture.hpp"
#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

electrum_setup_fixture::electrum_setup_fixture(const initializer& setup,
    bool address_index, const configurator& configure, service which)
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
            if (!address_index)
                config_.database.outs.buckets = 0;

            config_.database.path = TEST_DIRECTORY;
            return config_.database;
        }()
    },
    query_{ store_ }, which_{ which }, log_{},
    server_{ query_, config_, log_ }
{
    test::clear(test::directory);

    auto& database_settings = config_.database;
    auto& network_settings = config_.network;
    auto& node_settings = config_.node;
    auto& server_settings = config_.server;

    // Only the configured service binds.
    const auto sparrow = (which == service::sparrow);
    auto& electrum = sparrow ?
        static_cast<server::settings::electrum_server&>(server_settings.sparrow) :
        server_settings.electrum;

    electrum.binds = { { sparrow ? SPARROW_ENDPOINT : ELECTRUM_ENDPOINT } };
    electrum.server_name = "server_name";
    electrum.banner_message = "banner_message";
    electrum.donation_address = "donation_address";
    electrum.maximum_subscriptions = 2;
    electrum.maximum_history = 5;
    electrum.maximum_headers = 5;
    electrum.connections = 1;
    electrum.inactivity_minutes = 1;
    database_settings.interval_depth = 2;
    node_settings.delay_inbound = false;
    node_settings.minimum_fee_rate = 99.0;
    node_settings.fee_estimate_horizon = 8;
    network_settings.inbound.connections = 0;
    network_settings.outbound.connections = 0;

    // Apply test-specific configuration overrides.
    if (configure)
        configure(config_);

    // Create and populate the store.
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    setup(query_);

    std::promise<code> started{};
    server_.start([&](const code& ec) NOEXCEPT
    {
        started.set_value(ec);
    });

    // Block until server is started.
    ec = started.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());

    std::promise<code> running{};
    server_.run([&](const code& ec) NOEXCEPT
    {
        running.set_value(ec);
    });

    // Block until server is running.
    ec = running.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    socket_.connect(electrum.binds.back().to_endpoint());
}

electrum_setup_fixture::~electrum_setup_fixture()
{
    socket_.close();
    server_.close();
    const auto ec = store_.close([](auto, auto){});
    BOOST_WARN_MESSAGE(!ec, ec.message());
    test::clear(test::directory);
}

BC_POP_WARNING()

int64_t electrum_setup_fixture::get_error(const std::string& request)
{
    try
    {
        return get(request).at("error").as_object().at("code").as_int64();
    }
    catch (const boost::system::system_error&)
    {
        return -1;
    }
}

boost::json::value electrum_setup_fixture::get(const std::string& request)
{
    socket_.send(boost::asio::buffer(request));
    return receive();
}

boost::json::value electrum_setup_fixture::receive()
{
    try
    {
        boost::asio::read_until(socket_, stream_, '\n');
    }
    catch (const boost::system::system_error&)
    {
        ////std::cout << "electrum::get -> dropped" << std::endl;
        return boost::json::parse(R"({"dropped":true})");
    }

    try
    {
        std::string response{};
        std::istream response_stream{ &stream_ };
        std::getline(response_stream, response);
        return boost::json::parse(response);
    }
    catch (const boost::system::system_error&)
    {
        ////std::cout << "electrum::parse -> " << e.what() << std::endl;
        return {};
    }
}

void electrum_setup_fixture::notify(node::chase event_, node::event_value value)
{
    server_.notify(error::success, event_, value);
}

const server::settings::electrum_server&
electrum_setup_fixture::options() const
{
    return which_ == service::sparrow ?
        static_cast<const server::settings::electrum_server&>(
            config_.server.sparrow) :
        config_.server.electrum;
}

bool electrum_setup_fixture::verify(const boost::json::value& response,
    electrum::version version, network::rpc::code_t id) const
{
    try
    {
        if (response.at("id").as_int64() != id)
            return false;

        // Assumes server always accepts proposed version.
        const auto& result = response.at("result").as_array();
        return (result.size() == two) &&
            (result.at(0).is_string() && result.at(1).is_string()) &&
            (result.at(0).as_string() == options().server_name) &&
            (result.at(1).as_string() == electrum::version_to_string(version));

    }
    catch (...)
    {
        return false;
    }
}

static std::string version_request(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return (boost_format
    (
        R"({"id":%1%,"method":"server.version","params":["%2%","%3%"]})"
    ) % id % name % electrum::version_to_string(version)).str();
}

bool electrum_setup_fixture::handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return verify(get(version_request(version, name, id) + "\n"), version, id);
}

// http POST.
// ----------------------------------------------------------------------------

boost::json::value electrum_setup_fixture::post(const std::string& request)
{
    namespace http = boost::beast::http;
    http::request<http::string_body> out{ http::verb::post, "/",
        network::http::version_1_1 };
    out.set(http::field::host, "localhost");
    out.set(http::field::content_type, "application/json");
    out.body() = request;
    out.prepare_payload();
    out.keep_alive(true);

    network::boost_code ec{};
    http::write(socket_, out, ec);
    if (ec)
        return boost::json::parse(R"({"dropped":true})");

    boost::beast::flat_buffer buffer{};
    http::response<http::string_body> in{};
    http::read(socket_, buffer, in, ec);
    if (ec)
        return boost::json::parse(R"({"dropped":true})");

    return test::parse_json(in.body());
}

bool electrum_setup_fixture::post_handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return verify(post(version_request(version, name, id)), version, id);
}

// websocket.
// ----------------------------------------------------------------------------

network::boost_code electrum_setup_fixture::ws_upgrade()
{
    network::boost_code ec{};
    BOOST_CHECK(!websocket_.has_value());

    websocket_.emplace(socket_);
    websocket_.value().text(true);
    websocket_.value().handshake("localhost", "/", ec);

    // A refused upgrade leaves the connection in http (teardown as such).
    if (ec)
        websocket_.reset();

    return ec;
}

boost::json::value electrum_setup_fixture::ws_receive()
{
    network::boost_code ec{};
    BOOST_CHECK(websocket_.has_value());

    boost::beast::flat_buffer buffer{};
    websocket_.value().read(buffer, ec);
    if (ec)
        return boost::json::parse(R"({"dropped":true})");

    return test::parse_json(boost::beast::buffers_to_string(buffer.data()));
}

boost::json::value electrum_setup_fixture::ws_get(const std::string& request)
{
    network::boost_code ec{};
    BOOST_CHECK(websocket_.has_value());
    websocket_.value().write(boost::asio::buffer(request), ec);
    if (ec)
        return boost::json::parse(R"({"dropped":true})");

    return ws_receive();
}

bool electrum_setup_fixture::ws_handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return verify(ws_get(version_request(version, name, id)), version, id);
}
