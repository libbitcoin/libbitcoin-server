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
#include "../blocks.hpp"
#include "electrum.hpp"
#include <future>
#include <boost/format.hpp>

electrum_setup_fixture::electrum_setup_fixture(const initializer& setup)
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
    BOOST_REQUIRE_MESSAGE(test::clear(test::directory), "electrum setup");

    auto& database_settings = config_.database;
    auto& network_settings = config_.network;
    auto& node_settings = config_.node;
    auto& server_settings = config_.server;
    auto& electrum = server_settings.electrum;

    electrum.binds = { { ELECTRUM_ENDPOINT } };
    electrum.server_name = "server_name";
    electrum.banner_message = "banner_message";
    electrum.donation_address = "donation_address";
    electrum.maximum_subscriptions = 3;
    electrum.maximum_headers = 5;
    electrum.connections = 1;
    database_settings.interval_depth = 2;
    node_settings.delay_inbound = false;
    node_settings.minimum_fee_rate = 99.0;
    network_settings.inbound.connections = 0;
    network_settings.outbound.connections = 0;

    // Create and populate the store.
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    BOOST_REQUIRE_MESSAGE(setup(query_), "electrum initialize");

    // Run the server.
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
    BOOST_WARN_MESSAGE(test::clear(test::directory), "electrum cleanup");
}

boost::json::value electrum_setup_fixture::get(const std::string& request)
{
    socket_.send(boost::asio::buffer(request));
    boost::asio::streambuf stream{};

    try
    {
        boost::asio::read_until(socket_, stream, '\n');
    }
    catch (const boost::system::system_error&)
    {
        ////BOOST_WARN_MESSAGE(false, e.what());
        return boost::json::parse(R"({"dropped":true})");
    }

    std::string response{};
    std::istream response_stream{ &stream };
    std::getline(response_stream, response);
    return boost::json::parse(response);
}

bool electrum_setup_fixture::handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    const auto request = boost::format
    (
        R"({"id":%1%,"method":"server.version","params":["%2%","%3%"]})" "\n"
    ) % id % name % electrum::version_to_string(version);

    const auto response = get(request.str());
    try
    {
        if (response.at("id").as_int64() != id)
            return false;

        // Assumes server always accepts proposed version.
        const auto& result = response.at("result").as_array();
        return (result.size() == two) &&
            (result.at(0).is_string() && result.at(1).is_string()) &&
            (result.at(0).as_string() == config_.server.electrum.server_name) &&
            (result.at(1).as_string() == electrum::version_to_string(version));

    }
    catch (...)
    {
        return false;
    }
}
