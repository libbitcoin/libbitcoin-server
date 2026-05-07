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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_NATIVE_NATIVE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_NATIVE_NATIVE

#include "../../test.hpp"
#include "../blocks.hpp"

#define NATIVE_ENDPOINT "127.0.0.1:65001"

// TODO: native is http so use boost::beast.

struct native_setup_fixture
{
    DELETE_COPY_MOVE(native_setup_fixture);

    using initializer = std::function<bool(test::query_t&)>;
    explicit native_setup_fixture(const initializer& setup);
    ~native_setup_fixture();

    bool expect_dropped(std::string_view target);
    std::string get_text(std::string_view target);
    system::data_chunk get_data(std::string_view target);
    boost::json::value get_json(std::string_view target);
    boost::beast::http::status get_status(std::string_view target);

protected:
    server::configuration config_;
    test::store_t store_;
    test::query_t query_;

private:
    using string_body = network::http::string_body;
    using string_request = boost::beast::http::request<string_body>;
    static string_request create_request(std::string_view target);

    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    boost::asio::ip::tcp::socket socket_{ io };
};

struct native_ten_block_setup_fixture
  : native_setup_fixture
{
    inline native_ten_block_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

#endif
