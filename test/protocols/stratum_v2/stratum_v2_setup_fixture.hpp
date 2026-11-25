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

/// The share difficulty of the fixtures, below that of the largest target.
constexpr double stratum_v2_minimum_difficulty = 1.0e-12;

struct stratum_v2_setup_fixture
  : rpc_setup_fixture
{
    DELETE_COPY_MOVE(stratum_v2_setup_fixture);

    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;
    using setup_connection = network::messages::sv2::setup_connection;
    using setup_connection_error = network::messages::sv2::setup_connection_error;
    using extended_success = network::messages::sv2::open_extended_mining_channel_success;
    using standard_success = network::messages::sv2::open_standard_mining_channel_success;
    using extended_job = network::messages::sv2::new_extended_mining_job;
    using standard_job = network::messages::sv2::new_mining_job;
    using prev_hash = network::messages::sv2::set_new_prev_hash;
    using extended_share = network::messages::sv2::submit_shares_extended;
    using standard_share = network::messages::sv2::submit_shares_standard;

    /// An open extended channel, its first job and previous block.
    struct extended
    {
        extended_success::cptr channel;
        extended_job::cptr job;
        prev_hash::cptr previous;
    };

    /// An open standard channel, its first job and previous block.
    struct standard
    {
        standard_success::cptr channel;
        standard_job::cptr job;
        prev_hash::cptr previous;
    };

    /// The largest target (every share meets it).
    static const system::hash_digest maximum_target;

    explicit stratum_v2_setup_fixture(const initializer& setup,
        const configurator& configure={});
    ~stratum_v2_setup_fixture();

    /// The address of a p2kh output of the fill, with the wallet's prefixes.
    static std::string address(uint8_t fill,
        system::chain::selection network=system::chain::selection::mainnet);

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

    // Send the message as a frame.
    template <class Message>
    void send_message(const Message& message)
    {
        send(to_frame(message));
    }

    // Receive the message (null if another or dropped).
    template <class Message>
    typename Message::cptr receive_message()
    {
        using namespace network::messages::sv2;
        const auto frame = receive();
        const auto head = heading::deserialize(frame);
        if (!head || head->message_type != Message::identifier)
            return {};

        const auto start = std::next(frame.begin(), heading::size());
        return Message::deserialize(system::data_chunk{ start, frame.end() });
    }

    // The frame of a mining setup of the version range and flags.
    static system::data_chunk setup_frame(uint16_t minimum=2,
        uint16_t maximum=2, uint32_t flags=0, uint8_t protocol=0);

    // Send the frame, returning the setup error received (null if another).
    setup_connection_error::cptr get_error(const system::data_chunk& frame);

    // Upgrade and set up the connection, true if set up.
    bool setup();

    // Open a channel of the user (request 7).
    void open_extended(const std::string& user, float hash_rate=0.0f,
        const system::hash_digest& max_target=maximum_target,
        uint16_t min_extranonce=0);
    void open_standard(const std::string& user, float hash_rate=0.0f,
        const system::hash_digest& max_target=maximum_target);

    // Set up, open a channel of the address of 0x01 and receive its job.
    extended start_extended();
    standard start_standard();

    // A share of the first job with the nonce (and rolled extranonce).
    static extended_share to_share(const extended& started, uint32_t nonce,
        uint32_t sequence=0);
    static standard_share to_share(const standard& started, uint32_t nonce,
        uint32_t sequence=0);

    // Submit count distinct shares of the first job, true if each succeeds.
    bool submit_shares(const extended& started, size_t count);

    // Submit a tx to the pool.
    using rpc_setup_fixture::submit;

    // Find the nonce of the first job that meets the block target, and the
    // hash of its header.
    static std::pair<uint32_t, system::hash_digest> mine(
        const extended& started);
    static std::pair<uint32_t, system::hash_digest> mine(
        const standard& started);

    // Receive until a previous block, returning it (null after a bound).
    prev_hash::cptr receive_previous();

    // Wait (bounded) for the confirmed top to reach height.
    bool confirmed(size_t height);

private:
    rpc_client client_{ io_ };
};

/// Mainnet ten block store, every share above the block target accepted.
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

/// As ten block, with a configured payout address.
struct stratum_v2_payout_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_payout_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.payout_address = address(0x42);
        })
    {
    }
};

/// As ten block, with a minimum difficulty no share meets.
struct stratum_v2_difficult_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_difficult_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.minimum_difficulty = 1.0e30;
        })
    {
    }
};

/// As ten block, with a configured start difficulty.
struct stratum_v2_start_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_start_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.start_difficulty = 2.0;
        })
    {
    }
};

/// As ten block, with an automatic start difficulty over a share interval.
struct stratum_v2_vardiff_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_vardiff_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.share_interval_seconds = 10;
        })
    {
    }
};

/// As ten block, a start difficulty of one decaying over a one second
/// share interval.
struct stratum_v2_decay_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_decay_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.start_difficulty = 1.0;
            config.server.stratum_v2.share_interval_seconds = 1;
        })
    {
    }
};

/// As ten block, with one channel to a connection.
struct stratum_v2_one_channel_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_one_channel_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v2.maximum_channels = 1;
        })
    {
    }
};

/// Store with a pooled transaction, so the template has a branch and fees.
struct stratum_v2_broadcast_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_broadcast_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            return test::setup_broadcast_store(query);
        })
    {
    }
};

/// Regtest genesis store, where a share commonly meets the block target.
struct stratum_v2_regtest_setup_fixture
  : stratum_v2_setup_fixture
{
    inline stratum_v2_regtest_setup_fixture()
      : stratum_v2_setup_fixture([](test::query_t& query)
        {
            using namespace system::chain;
            const system::settings regtest{ selection::regtest };
            return query.initialize(regtest.genesis_block);
        }, [](configuration& config)
        {
            using namespace system::chain;
            config.bitcoin = system::settings{ selection::regtest };
            config.server.wallet = { selection::regtest };
            config.server.stratum_v2.payout_address =
                address(0x42, selection::regtest);
        })
    {
    }
};

#endif
