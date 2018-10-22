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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_EVENT_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_EVENT_HPP

#include <cstdint>

namespace libbitcoin {
namespace server {
namespace http {

enum class event : uint8_t
{
    read,
    write,
    listen,
    accepted,
    error,
    closing,
    websocket_frame,
    websocket_control_frame,
    json_rpc
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
