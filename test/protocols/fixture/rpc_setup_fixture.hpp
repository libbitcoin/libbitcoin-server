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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_FIXTURE_RPC_SETUP_FIXTURE_HPP
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_FIXTURE_RPC_SETUP_FIXTURE_HPP

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

/// Lifecycle common to universal json-rpc service fixtures: store, node and
/// running server. Service supplies its own settings through configurator,
/// applied last so it overrides defaults set here, and before store is
/// constructed so database settings take effect (store snapshots them).
struct rpc_setup_fixture
{
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;

    DELETE_COPY_MOVE(rpc_setup_fixture);

    /// Synthesize node chase event (e.g. block organized after direct
    /// query_.set/push_confirmed) without live p2p sync.
    void notify(node::event_value value);

    /// Submit the tx to the pool, as from a peer.
    code submit(const system::chain::transaction& tx);

protected:
    /// Creates and populates store, then starts (optionally) and runs server.
    /// Derived fixture connects its client on return.
    rpc_setup_fixture(const initializer& setup, const configurator& configure,
        bool address_index=true, bool start=false);
    ~rpc_setup_fixture();

    /// The endpoint to which the service is bound.
    network::io::endpoint binding();

    /// Codes with which handshake completes when channel stops before opener.
    template <class Session>
    std::vector<code> stop_handshake(const typename Session::options_t& options)
    {
        struct accessor
          : Session
        {
            using Session::Session;
            using Session::attach_handshake;
        };

        network::threadpool pool{ 1 };
        const network::socket::parameters params{ .maximum_request = 42 };
        const auto session = std::make_shared<accessor>(server_, 42, config_,
            options);

        const auto socket = std::make_shared<network::socket>(log_,
            pool.service(), params);

        using channel_t = typename Session::channel_t;
        const auto channel = std::make_shared<channel_t>(log_, socket, 42,
            config_, options);

        std::vector<code> codes{};
        network::io::post(channel->strand(), [&]() NOEXCEPT
        {
            session->attach_handshake(channel, [&](const code& ec) NOEXCEPT
            {
                codes.push_back(ec);
            });

            channel->stop(network::error::peer_disconnect);
        });

        pool.stop();
        BC_REQUIRE(pool.join());

        return codes;
    }

    configuration config_;
    test::store_t store_;
    test::query_t query_;
    network::io::context io_{};

private:
    void teardown();

    network::logger log_;
    test::bound_server server_;
};

#endif
