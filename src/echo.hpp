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
#ifndef LIBBITCOIN_SERVER_ECHO_HPP
#define LIBBITCOIN_SERVER_ECHO_HPP

#include <sstream>

#define LOG_WORKER "worker"
#define LOG_REQUEST "request"

namespace server {

class stdout_wrapper
{
public:
    stdout_wrapper();
    stdout_wrapper(stdout_wrapper&& other);
    ~stdout_wrapper();

    template <typename T>
    stdout_wrapper& operator<<(T const& value)
    {
        stream_ << value;
        return *this;
    }

private:
    std::ostringstream stream_;
};

stdout_wrapper echo();

} // namespace server

#endif

