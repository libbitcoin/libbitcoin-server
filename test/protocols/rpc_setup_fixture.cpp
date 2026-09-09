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
#include "rpc_setup_fixture.hpp"

#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

rpc_setup_fixture::rpc_setup_fixture(const initializer& setup,
    const configurator& configure, bool address_index, bool start)
  : config_
    {
        system::chain::selection::mainnet,
        test::web_pages,
        test::web_pages
    },
    store_
    {
        [&]() NOEXCEPT -> const database::settings&
        {
            config_.database.path = TEST_DIRECTORY;
            config_.database.interval_depth = 2;
            if (!address_index)
                config_.database.outs.buckets = 0;

            config_.node.delay_inbound = false;
            config_.node.minimum_fee_rate = 99.0;
            config_.network.inbound.connections = 0;
            config_.network.outbound.connections = 0;

            // Last, so the service settings and any test override win, and
            // before construction, as the store snapshots these settings.
            if (configure)
                configure(config_);

            return config_.database;
        }()
    },
    query_{ store_ }, log_{},
    server_{ query_, config_, log_ }
{
    test::clear(test::directory);
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());

    setup(query_);

    // The node (chasers and address pool), bypassed by default.
    if (start)
    {
        std::promise<code> started{};
        server_.start([&](const code& ec) NOEXCEPT
        {
            started.set_value(ec);
        });

        ec = started.get_future().get();
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    }

    std::promise<code> running{};
    server_.run([&](const code& ec) NOEXCEPT
    {
        running.set_value(ec);
    });

    ec = running.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
}

rpc_setup_fixture::~rpc_setup_fixture()
{
    server_.close();
    const auto ec = store_.close([](auto, auto){});
    BOOST_WARN_MESSAGE(!ec, ec.message());

    test::clear(test::directory);
}

void rpc_setup_fixture::notify(node::chase event_, node::event_value value)
{
    server_.notify(system::error::success, event_, value);
}

BC_POP_WARNING()
