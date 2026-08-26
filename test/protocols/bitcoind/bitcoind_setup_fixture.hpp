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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_BITCOIND_BITCOIND_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_BITCOIND_BITCOIND_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

#define BITCOIND_ENDPOINT "127.0.0.1:65003"
#define BITCOIND_TEST_USERNAME "user"
#define BITCOIND_TEST_PASSWORD "pass"
#define BITCOIND_TEST_SCOPED_METHOD "getblockcount"

struct bitcoind_setup_fixture
{
    using status = boost::beast::http::status;
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;

    DELETE_COPY_MOVE(bitcoind_setup_fixture);
    explicit bitcoind_setup_fixture(const initializer& setup,
        const configurator& configure={});
    ~bitcoind_setup_fixture();

    // JSON-RPC 2.0 over HTTP POST to "/". params is a raw json value (array or
    // object). Returns the parsed json-rpc response object (with result/error).
    boost::json::value rpc(std::string_view method, std::string_view params="[]");

    // JSON-RPC over HTTP POST to "/" with a raw body (e.g. a batch). Returns
    // the parsed json response, or {"dropped":true} if the channel dropped.
    boost::json::value rpc_body(std::string_view body);

    // As rpc(), with basic authorization, returning only the http status.
    status rpc_status(std::string_view method, const std::string& username,
        const std::string& password);

    // Upgrade the connection to websocket (no further http requests).
    network::boost_code ws_upgrade();

    // As ws_upgrade(), with basic authorization on the upgrade request.
    network::boost_code ws_upgrade(const std::string& username,
        const std::string& password);

    // As rpc(), over the upgraded websocket connection.
    boost::json::value ws_rpc(std::string_view method,
        std::string_view params="[]");

    // As ws_rpc(), returning {"dropped":true} if the channel dropped.
    boost::json::value ws_rpc_dropped(std::string_view method,
        std::string_view params="[]");


    // bitcoind REST over HTTP GET (target under "/rest/...").
    status rest_status(std::string_view target);
    boost::json::value rest_json(std::string_view target);
    std::string rest_text(std::string_view target);
    system::data_chunk rest_data(std::string_view target);

protected:
    configuration config_;
    test::store_t store_;
    test::query_t query_;

private:
    using string_body = network::http::string_body;
    using string_request = boost::beast::http::request<string_body>;
    static string_request create_get(std::string_view target);
    static string_request create_post(std::string_view target,
        std::string_view body);

    using tcp_stream = boost::beast::tcp_stream;
    using websocket_stream = boost::beast::websocket::stream<tcp_stream&>;

    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    tcp_stream socket_{ io.get_executor() };
    std::optional<websocket_stream> websocket_{};
};

struct bitcoind_ten_block_setup_fixture
    : bitcoind_setup_fixture
{
    inline bitcoind_ten_block_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

// Configured with a credential -- for tests that verify authorization is
// established by the upgrade request, as ws frames carry no headers.
struct bitcoind_credentialed_setup_fixture
  : bitcoind_setup_fixture
{
    inline bitcoind_credentialed_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.bitcoind.credentials =
            {
                { BITCOIND_TEST_USERNAME ":" BITCOIND_TEST_PASSWORD }
            };
        })
    {
    }
};

// Configured with a credential scoped to a single method -- for tests that
// verify permitted() refuses at the transport (post 403, websocket stop).
struct bitcoind_scoped_credential_setup_fixture
  : bitcoind_setup_fixture
{
    inline bitcoind_scoped_credential_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.bitcoind.credentials =
            {
                { BITCOIND_TEST_USERNAME ":" BITCOIND_TEST_PASSWORD ":"
                    BITCOIND_TEST_SCOPED_METHOD }
            };
        })
    {
    }
};

// Configured with no currency window -- for tests of state that requires the
// confirmed top to be current (the test store is historical).
struct bitcoind_current_setup_fixture
  : bitcoind_setup_fixture
{
    inline bitcoind_current_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.node.currency_window_minutes = 0;
        })
    {
    }
};

// Configured with no address index -- for tests of the utxo set scan path.
struct bitcoind_no_address_setup_fixture
  : bitcoind_setup_fixture
{
    inline bitcoind_no_address_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.database.outs.buckets = 0;
        })
    {
    }
};

struct bitcoind_witness_setup_fixture
    : bitcoind_setup_fixture
{
    inline bitcoind_witness_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_witness_store(query);
        })
    {
    }
};

#endif
