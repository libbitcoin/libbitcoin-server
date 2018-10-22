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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_WEBSOCKET_TRANSFER_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_WEBSOCKET_TRANSFER_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace http {

struct BCS_API websocket_transfer
{
    bool in_progress;
    size_t offset;
    size_t length;
    size_t header_length;
    data_chunk mask;
    data_chunk data;
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
