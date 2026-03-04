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
#include "../test.hpp"
#include "blocks.hpp"
#include <future>
#include <boost/format.hpp>

#define ELECTRUM_ENDPOINT "127.0.0.1:65000"

struct electrum_setup_fixture
{
    DELETE_COPY_MOVE(electrum_setup_fixture);

    inline electrum_setup_fixture()
      : config_{ system::chain::selection::mainnet, native, admin },
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
        BOOST_REQUIRE(test::clear(test::directory));

        auto& database_settings = config_.database;
        auto& network_settings = config_.network;
        auto& node_settings = config_.node;
        auto& server_settings = config_.server;
        auto& electrum = server_settings.electrum;

        // >>>>>>>>>>>>>>> REQUIRES LOCALHOST TCP PORT 65000. <<<<<<<<<<<<<<<
        electrum.binds = { { ELECTRUM_ENDPOINT } };
        electrum.maximum_headers = 5;
        electrum.connections = 1;
        database_settings.interval_depth = 2;
        node_settings.delay_inbound = false;
        network_settings.inbound.connections = 0;
        network_settings.outbound.connections = 0;

        // Create and populate the store.
        BOOST_REQUIRE(!store_.create([](auto, auto){}));
        BOOST_REQUIRE(setup_eight_block_store());

        // Run the server.
        std::promise<code> running{};
        server_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        // Block until server is running.
        BOOST_REQUIRE(!running.get_future().get());
        socket_.connect(electrum.binds.back().to_endpoint());
    }

    inline ~electrum_setup_fixture()
    {
        socket_.close();
        server_.close();
        BOOST_REQUIRE(!store_.close([](auto, auto){}));
        BOOST_REQUIRE(test::clear(test::directory));
    }

    inline const configuration& config() const NOEXCEPT
    {
        return config_;
    }

    inline auto get(const std::string& request)
    {
        socket_.send(boost::asio::buffer(request));
        boost::asio::streambuf stream{};
        read_until(socket_, stream, '\n');

        std::string response{};
        std::istream response_stream{ &stream };
        std::getline(response_stream, response);

        return boost::json::parse(response);
    }

    inline bool handshake(const std::string& version="1.4",
        const std::string& name="test", network::rpc::code_t id=0)
    {
        const auto request = boost::format
        (
            R"({"id":%1%,"method":"server.version","params":["%2%","%3%"]})" "\n"
        ) % id % name % version;

        const auto response = get(request.str());
        if (!response.at("result").is_array() ||
            !response.at("id").is_int64() ||
             response.at("id").as_int64() != id)
            return false;

        // Assumes server always accept proposed version.
        const auto& result = response.at("result").as_array();
        return (result.size() == two) &&
            (result.at(0).is_string() && result.at(1).is_string()) &&
            (result.at(0).as_string() == config().network.user_agent) &&
            (result.at(1).as_string() == version);
    }

private:
    bool setup_eight_block_store() NOEXCEPT
    {
        using namespace database;
        return query_.initialize(genesis) &&
            query_.set(block1, context{ 0, 1, 0 }, false, false) &&
            query_.set(block2, context{ 0, 2, 0 }, false, false) &&
            query_.set(block3, context{ 0, 3, 0 }, false, false) &&
            query_.set(block4, context{ 0, 4, 0 }, false, false) &&
            query_.set(block5, context{ 0, 5, 0 }, false, false) &&
            query_.set(block6, context{ 0, 6, 0 }, false, false) &&
            query_.set(block7, context{ 0, 7, 0 }, false, false) &&
            query_.set(block8, context{ 0, 8, 0 }, false, false) &&
            query_.set(block9, context{ 0, 9, 0 }, false, false) &&
            query_.push_confirmed(query_.to_header(block1_hash), false) &&
            query_.push_confirmed(query_.to_header(block2_hash), false) &&
            query_.push_confirmed(query_.to_header(block3_hash), false) &&
            query_.push_confirmed(query_.to_header(block4_hash), false) &&
            query_.push_confirmed(query_.to_header(block5_hash), false) &&
            query_.push_confirmed(query_.to_header(block6_hash), false) &&
            query_.push_confirmed(query_.to_header(block7_hash), false) &&
            query_.push_confirmed(query_.to_header(block8_hash), false) &&
            query_.push_confirmed(query_.to_header(block9_hash), false);
    }

    configuration config_;
    store_t store_;
    query_t query_;
    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    boost::asio::ip::tcp::socket socket_{ io };
};
