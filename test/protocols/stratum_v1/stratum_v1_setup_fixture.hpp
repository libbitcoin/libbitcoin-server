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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_STRATUM_V1_STRATUM_V1_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_STRATUM_V1_STRATUM_V1_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "../fixture/rpc_client.hpp"
#include "../fixture/rpc_setup_fixture.hpp"

#define STRATUM_V1_ENDPOINT "127.0.0.1"

/// The share difficulty of the fixtures, below that of the largest target.
constexpr double stratum_v1_minimum_difficulty = 1.0e-12;

struct stratum_v1_setup_fixture
  : rpc_setup_fixture
{
    DELETE_COPY_MOVE(stratum_v1_setup_fixture);

    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;

    /// A share that meets the block target, and the hash of its header.
    struct mined
    {
        std::string nonce;
        system::hash_digest hash;
    };

    explicit stratum_v1_setup_fixture(const initializer& setup,
        const configurator& configure={});
    ~stratum_v1_setup_fixture();

    /// The address of a p2kh output of the fill, with the wallet's prefixes.
    static std::string address(uint8_t fill,
        system::chain::selection network=system::chain::selection::mainnet);

    // json-rpc over the raw tcp stream (downgrades the connection).
    system::json::value get(const std::string& request);
    system::json::value receive();
    int64_t get_error(const std::string& request);

    // json-rpc over http POST to "/" (the connection remains http).
    system::json::value post(const std::string& request);

    // json-rpc over the upgraded websocket connection.
    network::code ws_upgrade();
    system::json::value ws_get(const std::string& request);
    system::json::value ws_receive();

    // Subscribe, retaining extranonce1, and authorize the worker.
    system::json::value subscribe();
    system::json::value authorize(const std::string& worker);

    // Subscribe and authorize, returning the first job (after difficulty).
    system::json::value start_job();

    // Submit a share of the job (notify params), as the authorized worker.
    system::json::value submit(const system::json::value& job,
        const std::string& extranonce2, const std::string& nonce,
        const std::string& version_bits={});

    // Submit count distinct shares of the job, true if each is accepted.
    bool submit_shares(const system::json::value& job, size_t count);

    // Find a share of the job that meets the block target.
    mined mine(const system::json::value& job,
        const std::string& extranonce2) const;

    // Receive until a clean job, returning it (null after a bound).
    system::json::value receive_clean();

    // Wait (bounded) for the confirmed top to reach height.
    bool confirmed(size_t height);

    // The worker authorized by subscribe and start_job.
    std::string worker() const;

private:
    rpc_client client_{ io_ };
    std::string extranonce1_{};
    std::string worker_{};
};

/// Mainnet ten block store, every share above the block target accepted.
struct stratum_v1_ten_block_setup_fixture
  : stratum_v1_setup_fixture
{
    inline stratum_v1_ten_block_setup_fixture()
      : stratum_v1_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

/// As ten block, with a configured payout address.
struct stratum_v1_payout_setup_fixture
  : stratum_v1_setup_fixture
{
    inline stratum_v1_payout_setup_fixture()
      : stratum_v1_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v1.payout_address = address(0x42);
        })
    {
    }
};

/// As ten block, with a minimum difficulty no share meets.
struct stratum_v1_difficult_setup_fixture
  : stratum_v1_setup_fixture
{
    inline stratum_v1_difficult_setup_fixture()
      : stratum_v1_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v1.minimum_difficulty = 1.0e30;
        })
    {
    }
};

/// As ten block, with difficulty retargeted toward a share interval.
struct stratum_v1_vardiff_setup_fixture
  : stratum_v1_setup_fixture
{
    inline stratum_v1_vardiff_setup_fixture()
      : stratum_v1_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.stratum_v1.share_interval_seconds = 10;
        })
    {
    }
};

/// Store with a pooled transaction, so the template has a branch and fees.
struct stratum_v1_broadcast_setup_fixture
  : stratum_v1_setup_fixture
{
    inline stratum_v1_broadcast_setup_fixture()
      : stratum_v1_setup_fixture([](test::query_t& query)
        {
            return test::setup_broadcast_store(query);
        })
    {
    }
};

/// Regtest genesis store, where a share commonly meets the block target.
struct stratum_v1_regtest_setup_fixture
  : stratum_v1_setup_fixture
{
    inline stratum_v1_regtest_setup_fixture()
      : stratum_v1_setup_fixture([](test::query_t& query)
        {
            using namespace system::chain;
            const system::settings regtest{ selection::regtest };
            return query.initialize(regtest.genesis_block);
        }, [](configuration& config)
        {
            using namespace system::chain;
            config.bitcoin = system::settings{ selection::regtest };
            config.server.wallet = { selection::regtest };
            config.server.stratum_v1.payout_address =
                address(0x42, selection::regtest);
        })
    {
    }
};

#endif
