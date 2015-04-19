/*
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
#include "echo.hpp"

#include <iostream>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace server {

stdout_wrapper::stdout_wrapper()
{
}
stdout_wrapper::stdout_wrapper(stdout_wrapper&& other)
  : stream_(other.stream_.str())
{
}
stdout_wrapper::~stdout_wrapper()
{
    bc::cout << stream_.str() << std::endl;
}

stdout_wrapper echo()
{
    return stdout_wrapper();
}

} // namespace server
} // namespace libbitcoin
