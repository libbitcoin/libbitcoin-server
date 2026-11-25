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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_STRATUM_V2_STRATUM_V2_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_STRATUM_V2_STRATUM_V2_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "../fixture/rpc_client.hpp"
#include "../fixture/rpc_setup_fixture.hpp"

#define STRATUM_V2_ENDPOINT "127.0.0.1"

struct stratum_v2_setup_fixture
  : rpc_setup_fixture
{
    DELETE_COPY_MOVE(stratum_v2_setup_fixture);

    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;
    using setup_connection = network::messages::sv2::setup_connection;
    using setup_connection_error = network::messages::sv2::setup_connection_error;

    explicit stratum_v2_setup_fixture(const initializer& setup,
        const configurator& configure={});
    ~stratum_v2_setup_fixture();

    // Upgrade the connection to websocket.
    network::code upgrade();
    network::code upgrade(const std::string& username,
        const std::string& password);

    // The status of an http GET (the connection is not upgraded).
    network::http::status get_status();

    // One frame to each binary message (read empty if dropped).
    void send(const system::data_chunk& frame);
    system::data_chunk receive();

    // The frame of a message.
    template <class Message>
    static system::data_chunk to_frame(const Message& message)
    {
        const auto frame = network::messages::sv2::serialize(message);
        BC_REQUIRE(frame);
        return *frame;
    }

    // The frame of a mining setup of the version range and flags.
    static system::data_chunk setup_frame(uint16_t minimum=2,
        uint16_t maximum=2, uint32_t flags=0, uint8_t protocol=0);

    // Send the frame, returning the setup error received (null if another).
    setup_connection_error::cptr get_error(const system::data_chunk& frame);

private:
    rpc_client client_{ io_ };
};

/// Mainnet ten block store.
struct stratum_v2_ten_block_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_ten_block_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

/// As ten block, with a credential.
struct stratum_v2_credential_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_credential_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.credentials = { { "miner:pass" } };
        })
    {
    }
};

#endif
