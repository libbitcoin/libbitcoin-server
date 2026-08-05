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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_BTCD_BTCD_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_BTCD_BTCD_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

#define BTCD_ENDPOINT "127.0.0.1:65004"
#define BTCD_TEST_USERNAME "user"
#define BTCD_TEST_PASSWORD "pass"
#define BTCD_TEST_SCOPED_METHOD "session"

struct btcd_setup_fixture
{
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;

    DELETE_COPY_MOVE(btcd_setup_fixture);

    explicit btcd_setup_fixture(const initializer& setup,
        const configurator& configure={});
    ~btcd_setup_fixture();

    // JSON-RPC 1.0 over websocket. params must be a json array (json-rpc-v1
    // does not support 'null' or omitted params, use "[]" for no arguments).
    // Returns the parsed json-rpc response object (result/error).
    boost::json::value rpc(std::string_view method,
        std::string_view params = "[]");

    // As rpc(), returning the response's error code (-1 if not an error).
    int64_t rpc_error(std::string_view method,
        std::string_view params = "[]");

    // Authorize the connection (btcd's in-band credential method), for tests
    // that require it as a precondition -- mirrors electrum's handshake().
    bool authenticate(const std::string& username=BTCD_TEST_USERNAME,
        const std::string& password=BTCD_TEST_PASSWORD);

    // JSON-RPC 2.0 over plain HTTP POST to "/" (a separate connection from
    // the ws one rpc() uses).
    boost::json::value http_rpc(std::string_view method,
        std::string_view params = "[]");

    // Read one further (unprompted) server push, e.g. a blockconnected
    // notification. Returns the parsed json-rpc notification object.
    boost::json::value receive_notification();

    // Synthesize a node chase event (e.g. a block organized/confirmed after
    // a direct query_.set/push_confirmed) without a live p2p sync -- mirrors
    // electrum_setup_fixture::notify.
    void notify(node::chase event_, node::event_value value=0_u32);

protected:
    configuration config_;
    test::store_t store_;
    test::query_t query_;

private:
    using tcp_stream = boost::beast::tcp_stream;
    using websocket_stream = boost::beast::websocket::stream<tcp_stream&>;

    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io_{};
    tcp_stream socket_{ io_.get_executor() };
    websocket_stream websocket_{ socket_ };
    tcp_stream http_socket_{ io_.get_executor() };
    int http_request_id_{};
    int request_id_{};
};

struct btcd_ten_block_setup_fixture
  : btcd_setup_fixture
{
    inline btcd_ten_block_setup_fixture()
      : btcd_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

// Configured with an unscoped credential -- for tests that exercise the
// in-band 'authenticate' call itself (success, wrong-password rejection,
// calling another method before authenticating).
struct btcd_credentialed_setup_fixture
  : btcd_setup_fixture
{
    inline btcd_credentialed_setup_fixture()
      : btcd_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.btcd.credentials =
            {
                { BTCD_TEST_USERNAME ":" BTCD_TEST_PASSWORD }
            };
        })
    {
    }
};

// Configured with a credential scoped to a single method -- for tests that
// verify channel_http::permitted() enforces per-method credential scoping
// over ws once authenticated.
struct btcd_scoped_credential_setup_fixture
  : btcd_setup_fixture
{
    inline btcd_scoped_credential_setup_fixture()
      : btcd_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.btcd.credentials =
            {
                { BTCD_TEST_USERNAME ":" BTCD_TEST_PASSWORD ":"
                    BTCD_TEST_SCOPED_METHOD }
            };
        })
    {
    }
};

#endif
