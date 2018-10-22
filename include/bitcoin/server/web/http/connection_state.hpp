/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_CONNECTION_STATE_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_CONNECTION_STATE_HPP

namespace libbitcoin {
namespace server {
namespace http {

enum class connection_state
{
    error = -1,
    connecting = 99,
    connected = 100,
    listening = 101,
    ssl_handshake = 102,
    closed = 103,
    disconnect_immediately = 200,
    unknown
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
