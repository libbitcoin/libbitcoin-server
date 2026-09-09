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
#include "btcd_setup_fixture.hpp"
#include <future>
#include <sstream>

using namespace system;
using namespace boost::beast;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

btcd_setup_fixture::btcd_setup_fixture(const initializer& setup,
    bool address_index, const configurator& configure)
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
    query_{ store_ }, log_{},
    server_{ query_, config_, log_ }
{
    test::clear(test::directory);

    auto& database_settings = config_.database;
    auto& network_settings = config_.network;
    auto& node_settings = config_.node;
    auto& btcd = config_.server.btcd;

    btcd.binds = { { BTCD_ENDPOINT } };

    // 2: the ws connection, plus the plain one used by http_rpc or tcp_rpc.
    btcd.connections = 2;

    // Distinct from the [bitcoind] section default, so that a read of the
    // wrong service section is visible (see btcd_rpc__getnetworkinfo).
    btcd.subversion = "/libbitcoin:btcd/";
    btcd.inactivity_minutes = 1;
    database_settings.interval_depth = 2;
    node_settings.delay_inbound = false;
    node_settings.minimum_fee_rate = 99.0;
    network_settings.inbound.connections = 0;
    network_settings.outbound.connections = 0;

    // Apply test-specific configuration overrides.
    if (configure)
        configure(config_);

    // Create and populate the store.
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    setup(query_);

    // Run the server.
    std::promise<code> running{};
    server_.run([&](const code& ec) NOEXCEPT
    {
        running.set_value(ec);
    });

    // Block until server is running.
    ec = running.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());

    client_.connect(btcd.binds.back().to_endpoint());
    const auto wec = client_.upgrade();
    BOOST_REQUIRE_MESSAGE(!wec, wec.message());

    other_.connect(btcd.binds.back().to_endpoint());
}

btcd_setup_fixture::~btcd_setup_fixture()
{
    client_.close();
    other_.close();
    server_.close();
    const auto ec = store_.close([](auto, auto){});
    BOOST_WARN_MESSAGE(!ec, ec.message());
    test::clear(test::directory);
}

BC_POP_WARNING()

static std::string body_of(int id, std::string_view method,
    std::string_view params)
{
    std::ostringstream body{};
    body << R"({"jsonrpc":"1.0","id":)" << id
         << R"(,"method":")" << method
         << R"(","params":)" << params << "}";
    return body.str();
}

boost::json::value btcd_setup_fixture::rpc(std::string_view method,
    std::string_view params)
{
    return client_.frame(body_of(request_id_++, method, params), true);
}

int64_t btcd_setup_fixture::rpc_error(std::string_view method,
    std::string_view params)
{
    try
    {
        return rpc(method, params).at("error").as_object().at("code")
            .as_int64();
    }
    catch (const boost::system::system_error&)
    {
        return -1;
    }
}

bool btcd_setup_fixture::authenticate(const std::string& username,
    const std::string& password)
{
    const auto request = R"(["%1%","%2%"])";
    const auto response = rpc("authenticate",
        (boost_format(request) % username % password).str());

    try
    {
        return response.at("error").is_null();
    }
    catch (const boost::system::system_error&)
    {
        return false;
    }
}

boost::json::value btcd_setup_fixture::receive_notification()
{
    return client_.read_frame(true);
}

// Raw json on the plain socket, which the server detects and downgrades to a
// newline-delimited json-rpc stream. Mutually exclusive with http_rpc, as the
// detection is latched on the first read of the connection.
boost::json::value btcd_setup_fixture::tcp_rpc(std::string_view method,
    std::string_view params)
{
    return other_.send(body_of(http_request_id_++, method, params) + "\n");
}

boost::json::value btcd_setup_fixture::http_rpc(std::string_view method,
    std::string_view params)
{
    return other_.post(body_of(http_request_id_++, method, params), "/", true);
}

void btcd_setup_fixture::notify(node::chase event_, node::event_value value)
{
    server_.notify(system::error::success, event_, value);
}
