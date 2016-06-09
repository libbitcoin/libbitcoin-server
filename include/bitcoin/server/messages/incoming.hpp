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
#ifndef LIBBITCOIN_SERVER_INCOMING
#define LIBBITCOIN_SERVER_INCOMING

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class BCS_API incoming
{
public:
    /// A printable address for logging only.
    std::string address();

    /// Send a message from the socket.
    code receive(bc::protocol::zmq::socket& socket, bool secure=false);

    /// The message route as seen at workers.
    data_chunk address1;
    data_chunk address2;
    bool delimited;

    /// For deferred work, directs worker to respond on secure endpoint.
    bool secure;

    /// Query command (used for subscription, always returned to caller).
    std::string command;

    /// Structure is little-endian.
    /// Arbitrary caller data (returned to caller for correlation).
    uint32_t id;

    /// Serialized query (structure defined in relation to command).
    data_chunk data;
};

} // namespace server
} // namespace libbitcoin

#endif
