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
#include <string>
#include <bitcoin/protocol.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

std::string incoming::address()
{
    return "[" + encode_base16(address1) + "]";
}

// Protocol delimitation migration plan.
//-----------------------------------------------------------------------------
//    v1/v2 server: ROUTER, requires not delimited
//       v3 server: ROUTER, allows/echos delimited
// v1/v2/v3 client: DEALER (not framed)
//-----------------------------------------------------------------------------
//       v4 server: ROUTER, requires delimited
//       v4 client: DEALER (delimited) or REQ
//-----------------------------------------------------------------------------

// TODO: generalize address stripping, store all routes, use (hack) parameter
// of expected payload length for parsing undelimited addressing.
// BUGBUG: current implementation assumes client has not added any addresses.
// This probably prevents the use of generalized zeromq routing to the server.
code incoming::receive(zmq::socket& socket, bool secure)
{
    zmq::message message;
    auto ec = socket.receive(message);

    if (ec)
        return ec;
    
    if (message.size() < 5 || message.size() > 6)
        return error::bad_stream;

    // Client is undelimited DEALER -> 2 addresses with no delimiter.
    // Client is REQ or delimited DEALER -> 2 addresses with delimiter.
    address1 = message.dequeue_data();
    address2 = message.dequeue_data();

    // In the reply we echo the delimited-ness of the original request.
    delimited = message.size() == 4;

    if (delimited)
        message.dequeue();

    // All libbitcoin queries have these three frames.
    //-------------------------------------------------------------------------

    // Query command (returned to caller).
    command = message.dequeue_text();

    // Arbitrary caller data (returned to caller for correlation).
    if (!message.dequeue(id))
        return error::bad_stream;

    // Serialized query.
    data = message.dequeue_data();

    // For deferred work, directs worker to respond on secure endpoint.
    this->secure = secure;
    return error::success;
}

} // namespace server
} // namespace libbitcoin
