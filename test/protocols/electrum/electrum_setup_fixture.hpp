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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_ELECTRUM_ELECTRUM_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_ELECTRUM_ELECTRUM_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "../fixture/rpc_client.hpp"
#include "../fixture/rpc_setup_fixture.hpp"

#define ELECTRUM_ENDPOINT "127.0.0.1:65002"
#define SPARROW_ENDPOINT "127.0.0.1:65003"

struct electrum_setup_fixture
  : rpc_setup_fixture
{
    DELETE_COPY_MOVE(electrum_setup_fixture);

    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;
    /// The service configured and exercised (each has its own binding).
    enum class service { electrum, sparrow };

    explicit electrum_setup_fixture(const initializer& setup,
        bool address_index=true, const configurator& configure={},
        service which=service::electrum);
    ~electrum_setup_fixture();

    // json-rpc over the raw tcp stream (downgrades the connection).
    boost::json::value receive();
    int64_t get_error(const std::string& request);
    boost::json::value get(const std::string& request);
    bool handshake(electrum::version version,
        const std::string& name="test", network::rpc::code_t id={});

    // json-rpc over http POST to "/" (the connection remains http).
    boost::json::value post(const std::string& request);
    bool post_handshake(electrum::version version,
        const std::string& name="test", network::rpc::code_t id={});

    // Upgrade the connection to websocket (no further http requests).
    network::boost_code ws_upgrade();

    // json-rpc over the upgraded websocket connection.
    boost::json::value ws_get(const std::string& request);
    bool ws_handshake(electrum::version version,
        const std::string& name="test", network::rpc::code_t id={});

    // Read one unsolicited frame (notification) from the websocket.
    boost::json::value ws_receive();

private:
    // Verify the server.version response of any transport.
    bool verify(const boost::json::value& response, electrum::version version,
        network::rpc::code_t id) const;

    // The settings of the configured service (electrum or sparrow).
    const server::settings::electrum_server& options() const;

    const service which_;
    rpc_client client_{ io_ };
};

struct electrum_ten_block_setup_fixture
  : electrum_setup_fixture
{
    inline electrum_ten_block_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

/// The sparrow service, which reuses this harness (same transports, same
/// handshake, same electrum interface).
struct sparrow_ten_block_setup_fixture
  : electrum_setup_fixture
{
    inline sparrow_ten_block_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, true, {}, service::sparrow)
    {
    }
};

struct electrum_three_block_confirmed_address_setup_fixture
  : electrum_setup_fixture
{
    inline electrum_three_block_confirmed_address_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_confirmed_address_store(query);
        })
    {
    }
};

struct electrum_broadcast_setup_fixture
  : electrum_setup_fixture
{
    inline electrum_broadcast_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            return test::setup_broadcast_store(query);
        })
    {
    }
};

struct electrum_disabled_address_index_setup_fixture
  : electrum_setup_fixture
{
    inline electrum_disabled_address_index_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            return test::setup_three_block_confirmed_address_store(query);
        }, false)
    {
    }
};

struct electrum_restricted_version_setup_fixture
  : electrum_setup_fixture
{
    inline electrum_restricted_version_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, true, [](configuration& config)
        {
            // Restricted bounds, maximum is intentionally undefined (1.5).
            config.server.electrum.protocol_minimum = { 1, 2 };
            config.server.electrum.protocol_maximum = { 1, 5 };
        })
    {
    }
};

#endif
