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
#include "bitcoind.hpp"
#include <future>

bitcoind_setup_fixture::bitcoind_setup_fixture()
  : config_{ system::chain::selection::mainnet, test::web_pages, test::web_pages },
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
    auto ec = store_.create([](auto, auto) {});

    // Create and populate the store.
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    BOOST_REQUIRE_MESSAGE(test::setup_ten_block_store(query_), "bitcoind initialize");

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

boost::json::value bitcoind_setup_fixture::get(const std::string& request)
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
