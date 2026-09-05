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
#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "broadcast_setup_fixture.hpp"
#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

broadcast_setup_fixture::broadcast_setup_fixture(const initializer& setup,
    const system::data_chunk& curve_secret)
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
            return config_.database;
        }()
    },
    query_{ store_ }, log_{},
    server_{ query_, config_, log_ }
{
    test::clear(test::directory);

    auto& network_settings = config_.network;
    auto& node_settings = config_.node;
    auto& broadcast = config_.server.bitcoind_broadcast;

    broadcast.binds = { { BROADCAST_ENDPOINT } };
    broadcast.maximum_subscriptions = 2;
    broadcast.curve_secret = curve_secret;
    broadcast.connections = 1;
    broadcast.inactivity_minutes = 1;
    node_settings.delay_inbound = false;
    network_settings.inbound.connections = 0;
    network_settings.outbound.connections = 0;

    // Create and populate the store.
    auto ec = store_.create([](auto, auto) {});
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    setup(query_);

    std::promise<code> started{};
    server_.start([&](const code& ec) NOEXCEPT
    {
        started.set_value(ec);
    });

    // Block until server is started.
    ec = started.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());

    std::promise<code> running{};
    server_.run([&](const code& ec) NOEXCEPT
    {
        running.set_value(ec);
    });

    // Block until server is running.
    ec = running.get_future().get();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
    socket_.connect(broadcast.binds.back().to_endpoint());
}

broadcast_setup_fixture::~broadcast_setup_fixture()
{
    socket_.close();
    server_.close();
    const auto ec = store_.close([](auto, auto){});
    BOOST_WARN_MESSAGE(!ec, ec.message());
    test::clear(test::directory);
}

BC_POP_WARNING()

void broadcast_setup_fixture::notify(node::chase event_,
    node::event_value value)
{
    server_.notify(error::success, event_, value);
}
