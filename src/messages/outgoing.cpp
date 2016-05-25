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
#include <bitcoin/server/messages/incoming.hpp>

#include <cstdint>
#include <random>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/messages/outgoing.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

outgoing::outgoing(const incoming& request, const code& ec)
  : outgoing(request.command,
        build_chunk({ to_little_endian(static_cast<uint32_t>(ec.value())) }),
        request.address, request.id)
{
}

outgoing::outgoing(const incoming& request, const data_chunk& data)
  : outgoing(request.command, data, request.address, request.id)
{
}

outgoing::outgoing(const std::string& command, const data_chunk& data,
    const data_chunk& destination)
  : outgoing(command, data, destination, rand())
{
}

outgoing::outgoing(const std::string& command, const data_chunk& data,
    const data_chunk& destination, uint32_t id)
{
    // Optional, ROUTER sockets strip this.
    if (!destination.empty())
        message_.enqueue(destination);

    message_.enqueue(command);
    message_.enqueue_little_endian(id);
    message_.enqueue(data);
}

bool outgoing::send(zmq::socket& socket)
{
    return message_.send(socket);
}

} // namespace server
} // namespace libbitcoin
