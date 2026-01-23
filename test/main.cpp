/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#define BOOST_TEST_MODULE libbitcoin_server_test
#include <boost/test/unit_test.hpp>

#include <bitcoin/server.hpp>

// This is required due to the construction of a boost::asio::ssl::context in
// the configuration tree upon construct, which may preceed initialization.
struct global_fixture
{
    global_fixture() noexcept
    {
#if defined(HAVE_SSL)
        wolfSSL_Init();
#endif
    }

    ~global_fixture() noexcept
    {
#if defined(HAVE_SSL)
        wolfSSL_Cleanup();
#endif
    }
};

BOOST_GLOBAL_FIXTURE(global_fixture);
