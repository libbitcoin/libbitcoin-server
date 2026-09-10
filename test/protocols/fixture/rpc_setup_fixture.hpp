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

/// The lifecycle common to the universal json-rpc service fixtures: a store,
/// a node and a running server. A service supplies its own settings through
/// the configurator, which is applied last so that it overrides the defaults
/// set here, and before the store is constructed so that database settings
/// take effect (the store snapshots them).
struct rpc_setup_fixture
{
    using initializer = std::function<bool(test::query_t&)>;
    using configurator = std::function<void(configuration&)>;

    DELETE_COPY_MOVE(rpc_setup_fixture);

    /// Synthesize a node chase event (e.g. a block organized after a direct
    /// query_.set/push_confirmed) without a live p2p sync.
    void notify(node::chase event_, node::event_value value);

protected:
    /// Creates and populates the store, then starts (optionally) and runs
    /// the server. The derived fixture connects its client on return.
    rpc_setup_fixture(const initializer& setup, const configurator& configure,
        bool address_index=true, bool start=false);
    ~rpc_setup_fixture();

    configuration config_;
    test::store_t store_;
    test::query_t query_;
    boost::asio::io_context io_{};

private:
    network::logger log_;
    server::server_node server_;
};

#endif
