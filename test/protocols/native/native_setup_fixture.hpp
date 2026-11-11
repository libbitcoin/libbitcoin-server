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

#define NATIVE_ENDPOINT "127.0.0.1"
#define NATIVE_TEST_CREDENTIAL "user:pass:top"
#define NATIVE_TEST_AUTHORIZATION "Basic dXNlcjpwYXNz"

struct native_setup_fixture
{
    using status = network::http::status;
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(server::configuration&)>;

    DELETE_COPY_MOVE(native_setup_fixture);
    explicit native_setup_fixture(const initializer& setup,
        const configurator& configure={},
        const server::settings::embedded_pages& pages=test::web_pages);
    ~native_setup_fixture();

    bool expect_dropped(std::string_view target);
    status get_status(std::string_view target);
    status get_status(std::string_view target, network::http::field name,
        std::string_view value);

    // The full response, with an origin field unless empty.
    using string_response =
        network::http::message<false, network::http::string_body>;
    string_response get_response(std::string_view target,
        std::string_view origin={},
        uint32_t version=network::http::version_1_1);

    std::string get_text(std::string_view target);
    system::data_chunk get_data(std::string_view target);
    system::json::value get_json(std::string_view target);

    network::code ws_upgrade();
    void ws_send(std::string_view message);
    system::data_chunk ws_receive();
    std::string ws_get_text(std::string_view message);
    system::json::value ws_get_json(std::string_view message);
    system::data_chunk ws_get_data(std::string_view message);

    // 0_32 vs {} for xcode variant issue.
    void notify(node::event_value value);

protected:
    server::configuration config_;
    test::store_t store_;
    test::query_t query_;

private:
    using string_body = network::http::string_body;
    using string_request = network::http::message<true, string_body>;
    static string_request create_request(std::string_view target);

    using tcp_stream = network::io::socket;
    using websocket_stream = network::ws::client<network::io::socket>;

    void teardown();

    network::logger log_;
    test::bound_server server_;
    network::io::context io{};
    network::io::socket socket_{ io };
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

struct native_unconfirmed_setup_fixture
  : native_setup_fixture
{
    inline native_unconfirmed_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_unconfirmed_address_store(query);
        })
    {
    }
};

struct native_unassociated_setup_fixture
  : native_setup_fixture
{
    inline native_unassociated_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_unassociated_store(query);
        })
    {
    }
};

// A faulted store: block 1 is reassociated with the coinbase of block 2.
struct native_misassociated_setup_fixture
  : native_setup_fixture
{
    inline native_misassociated_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            const auto coinbase = test::block2.transactions_ptr()->front()->hash(false);
            return test::setup_ten_block_store(query) &&
                !query.set_code(query.to_header(test::block1_hash), database::tx_links{ query.to_tx(coinbase) }, false);
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

struct native_no_default_setup_fixture
  : native_setup_fixture
{
    inline native_no_default_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            const auto path = std::filesystem::absolute(TEST_DIRECTORY);
            config.server.native.path = path;
            config.server.native.default_ = {};
        })
    {
    }
};

struct native_hosts_setup_fixture
  : native_setup_fixture
{
    inline native_hosts_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.server.native.hosts = { { "example.com" } };
        })
    {
    }
};

struct native_origins_setup_fixture
  : native_setup_fixture
{
    inline native_origins_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.server.native.origins = { { "example.com" } };
        })
    {
    }
};

struct native_origins_hosts_setup_fixture
  : native_setup_fixture
{
    inline native_origins_hosts_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.server.native.hosts = { { "localhost" } };
            config.server.native.origins = { { "example.com" } };
        })
    {
    }
};

struct native_delayed_setup_fixture
  : native_setup_fixture
{
    inline native_delayed_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.node.delay_inbound = true;
        })
    {
    }
};

struct native_limited_setup_fixture
  : native_setup_fixture
{
    inline native_limited_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.node.limited_blocks = true;
            config.database.filter_bk.buckets = 0;
            config.database.filter_tx.buckets = 0;
        })
    {
    }
};

struct native_silent_setup_fixture
  : native_setup_fixture
{
    inline native_silent_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.node.silent_start_height = 42;
        })
    {
    }
};

// Runs a server node on demand, for start sequence outcomes.
struct server_node_setup_fixture
{
    using configurator = std::function<void(server::configuration&)>;

    DELETE_COPY_MOVE(server_node_setup_fixture);
    server_node_setup_fixture();
    ~server_node_setup_fixture();

    code run(const configurator& configure);

protected:
    server::configuration config_;
    test::store_t store_;
    test::query_t query_;
    network::logger log_;
    server::server_node server_;

private:
    void teardown();
};

// Configured with a credential scoped to the top method.
struct native_scoped_credential_setup_fixture
  : native_setup_fixture
{
    inline native_scoped_credential_setup_fixture()
      : native_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](server::configuration& config)
        {
            config.server.native.credentials = { { NATIVE_TEST_CREDENTIAL } };
        })
    {
    }
};

#endif
