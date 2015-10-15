/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <bitcoin/server.hpp>

using namespace bc;
using namespace bc::blockchain;
using namespace bc::network;
using namespace bc::node;
using namespace bc::server;

struct low_thread_priority_fixture
{
    low_thread_priority_fixture()
    {
        set_thread_priority(thread_priority::lowest);
    }

    ~low_thread_priority_fixture()
    {
        set_thread_priority(thread_priority::normal);
    }
};

static void uninitchain(const char prefix[])
{
    boost::filesystem::remove_all(prefix);
}

static void initchain(const char prefix[])
{
    uninitchain(prefix);
    boost::filesystem::create_directories(prefix);
    database::initialize(prefix, mainnet_genesis_block());
}

BOOST_FIXTURE_TEST_SUITE(server_tests, low_thread_priority_fixture)

BOOST_AUTO_TEST_CASE(server_test)
{
}

BOOST_AUTO_TEST_SUITE_END()
