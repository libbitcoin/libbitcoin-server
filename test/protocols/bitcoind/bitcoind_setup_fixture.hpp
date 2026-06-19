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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_BITCOIND_BITCOIND
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_BITCOIND_BITCOIND

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

#define BITCOIND_ENDPOINT "127.0.0.1:65000"

struct bitcoind_setup_fixture
{
    using status = boost::beast::http::status;
    using initializer = std::function<bool(test::query_t&)>;

    DELETE_COPY_MOVE(bitcoind_setup_fixture);
    explicit bitcoind_setup_fixture(const initializer& setup);
    ~bitcoind_setup_fixture();

    // JSON-RPC 2.0 over HTTP POST to "/". params is a raw json value (array or
    // object). Returns the parsed json-rpc response object (with result/error).
    boost::json::value rpc(std::string_view method, std::string_view params="[]");

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

    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    boost::beast::tcp_stream socket_{ io.get_executor() };
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
