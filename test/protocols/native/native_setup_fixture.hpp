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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_NATIVE_NATIVE_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_NATIVE_NATIVE_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

#define NATIVE_ENDPOINT "127.0.0.1:65000"

// TODO: native is http so use boost::beast.

struct native_setup_fixture
{
    using status = boost::beast::http::status;
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(server::configuration&)>;

    DELETE_COPY_MOVE(native_setup_fixture);
    explicit native_setup_fixture(const initializer& setup,
        const configurator& configure={},
        const server::settings::embedded_pages& pages=test::web_pages);
    ~native_setup_fixture();

    bool expect_dropped(std::string_view target);
    status get_status(std::string_view target);

    std::string get_text(std::string_view target);
    system::data_chunk get_data(std::string_view target);
    boost::json::value get_json(std::string_view target);

    network::boost_code ws_upgrade();
    system::data_chunk ws_receive();
    bool ws_dropped(std::string_view message);
    std::string ws_get_text(std::string_view message);
    boost::json::value ws_get_json(std::string_view message);
    system::data_chunk ws_get_data(std::string_view message);

    // 0_32 vs {} for xcode variant issue.
    void notify(node::event_value value);

protected:
    server::configuration config_;
    test::store_t store_;
    test::query_t query_;

private:
    using string_body = network::http::string_body;
    using string_request = boost::beast::http::request<string_body>;
    static string_request create_request(std::string_view target);

    using tcp_stream = boost::beast::tcp_stream;
    using websocket_stream = boost::beast::websocket::stream<tcp_stream&>;

    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    boost::beast::tcp_stream socket_{ io.get_executor() };
    std::optional<websocket_stream> websocket_{};
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

struct native_witness_setup_fixture
  : native_setup_fixture
{
    inline native_witness_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_witness_store(query);
        })
    {
    }
};

struct native_address_setup_fixture
  : native_setup_fixture
{
    inline native_address_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_confirmed_address_store(query);
        })
    {
    }
};

struct native_no_address_setup_fixture
  : native_setup_fixture
{
    inline native_no_address_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.database.outs.buckets = 0;
        })
    {
    }
};

struct native_pages
  : server::settings::embedded_pages
{
    server::span_value css() const NOEXCEPT override;
    server::span_value html() const NOEXCEPT override;
    server::span_value ecma() const NOEXCEPT override;
    server::span_value font() const NOEXCEPT override;
    server::span_value icon() const NOEXCEPT override;
};

extern const native_pages test_pages;

struct native_embedded_setup_fixture
  : native_setup_fixture
{
    inline native_embedded_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.server.native.path = {};
        }, test_pages)
    {
    }
};

struct native_file_setup_fixture
  : native_setup_fixture
{
    inline native_file_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            std::ofstream{ TEST_DIRECTORY + "/index.html" } << "<p>index</p>";
            std::ofstream{ TEST_DIRECTORY + "/page.css" } << "p{}";
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            const auto path = std::filesystem::absolute(TEST_DIRECTORY);
            config.server.native.path = path;
        })
    {
    }
};

#endif
