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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_BROADCAST_BROADCAST_SETUP_FIXTURE
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_BROADCAST_BROADCAST_SETUP_FIXTURE

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

#define BROADCAST_ENDPOINT "127.0.0.1:65003"

// A server with the bitcoind_broadcast (zmtp publisher) service bound, and a
// plain socket connected to it for a synchronous ZMTP subscriber peer.
struct broadcast_setup_fixture
{
    DELETE_COPY_MOVE(broadcast_setup_fixture);

    using initializer = std::function<bool(test::query_t&)>;
    explicit broadcast_setup_fixture(const initializer& setup,
        const system::data_chunk& curve_secret={});
    ~broadcast_setup_fixture();

    // 0_32 vs {} for xcode variant issue.
    void notify(node::chase event_, node::event_value value=0_u32);

protected:
    configuration config_;
    test::store_t store_;
    test::query_t query_;

    boost::asio::io_context io{};
    boost::asio::ip::tcp::socket socket_{ io };

private:
    network::logger log_;
    server::server_node server_;
};

struct broadcast_ten_block_setup_fixture
  : broadcast_setup_fixture
{
    inline broadcast_ten_block_setup_fixture()
      : broadcast_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        })
    {
    }
};

// The rfc7748 (section 6.1) bob secret key, as the CURVE server secret.
#define BROADCAST_CURVE_SECRET \
    "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"

struct broadcast_curve_ten_block_setup_fixture
  : broadcast_setup_fixture
{
    inline broadcast_curve_ten_block_setup_fixture()
      : broadcast_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, system::base16_chunk(BROADCAST_CURVE_SECRET))
    {
    }
};

#endif
