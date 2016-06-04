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

// Convert an error code to data for payload.
inline data_chunk to_chunk(const code& ec)
{
    return build_chunk(
    {
        to_little_endian(static_cast<uint32_t>(ec.value()))
    });
}

// Return an error code in response to the incoming query.
outgoing::outgoing(const incoming& request, const code& ec)
  : outgoing(request, to_chunk(ec))
{
}

// Return data in response to a successfully-executed incoming query.
outgoing::outgoing(const incoming& request, const data_chunk& data)
  : outgoing(request.command, data, request.address1, request.address2,
    request.delimited, request.id)
{
}

// Return data as a subscription by the given address (zero id).
outgoing::outgoing(const std::string& command, const data_chunk& data,
    const data_chunk& address1, const data_chunk& address2, bool delimited)
  : outgoing(command, data, address1, address2, delimited, 0)
{
}

// protected
outgoing::outgoing(const std::string& command, const data_chunk& data,
    const data_chunk& address1, const data_chunk& address2, bool delimited,
    uint32_t id)
{
    // Client is undelimited DEALER -> 2 addresses with no delimiter.
    // Client is REQ or delimited DEALER -> 2 addresses with delimiter.
    message_.enqueue(address1);
    message_.enqueue(address2);

    // In the reply we echo the delimited-ness of the original request.
    if (delimited)
        message_.enqueue();

    // All libbitcoin queries have these three frames.
    //-------------------------------------------------------------------------
    message_.enqueue(command);
    message_.enqueue_little_endian(id);
    message_.enqueue(data);
}

std::string outgoing::address()
{
    auto message = message_;
    return "[" + encode_base16(message.dequeue_data()) + "]";
}

code outgoing::send(zmq::socket& socket)
{
    return message_.send(socket);
}

} // namespace server
} // namespace libbitcoin
