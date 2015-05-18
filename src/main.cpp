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
#include <bitcoin/bitcoin.hpp>
#include "server.hpp"

BC_USE_LIBBITCOIN_MAIN

/**
 * Invoke this program with the raw arguments provided on the command line.
 * All console input and output streams for the application originate here.
 * @param argc  The number of elements in the argv array.
 * @param argv  The array of arguments, including the process.
 * @return      The numeric result to return via console exit.
 */
int bc::main(int argc, char* argv[])
{
#ifdef _MSC_VER
    if (_setmode(_fileno(stdin), _O_U8TEXT) == -1)
        throw std::exception("Could not set STDIN to utf8 mode.");
    if (_setmode(_fileno(stdout), _O_U8TEXT) == -1)
        throw std::exception("Could not set STDOUT to utf8 mode.");
    if (_setmode(_fileno(stderr), _O_U8TEXT) == -1)
        throw std::exception("Could not set STDERR to utf8 mode.");
#endif

    bc::set_thread_priority(bc::thread_priority::high);
    return bc::server::dispatch(argc, const_cast<const char**>(argv),
        bc::cin, bc::cout, bc::cerr);
}
