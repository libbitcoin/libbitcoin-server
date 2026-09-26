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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_ESPLORA_ESPLORA_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_ESPLORA_ESPLORA_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

#define ESPLORA_ENDPOINT "127.0.0.1:65005"

struct esplora_setup_fixture
{
    using status = boost::beast::http::status;
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(server::configuration&)>;

    DELETE_COPY_MOVE(esplora_setup_fixture);
    explicit esplora_setup_fixture(const initializer& setup,
        const configurator& configure={}, bool start=false);
    ~esplora_setup_fixture();

    status get_status(std::string_view target);
    std::string get_text(std::string_view target);
    system::data_chunk get_data(std::string_view target);
    boost::json::value get_json(std::string_view target);
    status post_status(std::string_view target, std::string_view body);
    std::string post_text(std::string_view target, std::string_view body);

    network::boost_code ws_upgrade();
    system::data_chunk ws_receive();
    bool ws_dropped(std::string_view message);
    std::string ws_get_text(std::string_view message);
    boost::json::value ws_get_json(std::string_view message);

protected:
    server::configuration config_;
    test::store_t store_;
    test::query_t query_;

private:
    using string_body = network::http::string_body;
    using string_request = boost::beast::http::request<string_body>;
    static string_request create_request(std::string_view target);
    static string_request create_post(std::string_view target,
        std::string_view body);

    using tcp_stream = boost::beast::tcp_stream;
    using websocket_stream = boost::beast::websocket::stream<tcp_stream&>;

    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    boost::beast::tcp_stream socket_{ io.get_executor() };
    std::optional<websocket_stream> websocket_{};
};

// Configured with the chasers started and no currency window -- for tests of
// transaction submission, which the tx chaser refuses unless the top is current.
struct esplora_submit_setup_fixture
  : esplora_setup_fixture
{
    inline esplora_submit_setup_fixture()
      : esplora_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.node.currency_window_minutes = 0;
        }, true)
    {
    }
};

struct esplora_ten_block_setup_fixture
  : esplora_setup_fixture
{
    inline esplora_ten_block_setup_fixture()
      : esplora_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

struct esplora_witness_setup_fixture
  : esplora_setup_fixture
{
    inline esplora_witness_setup_fixture()
      : esplora_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_witness_store(query);
        })
    {
    }
};

#endif
