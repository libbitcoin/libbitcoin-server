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
#include <bitcoin/server/messages/message.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/messages/route.hpp>
#include <bitcoin/server/messages/subscription.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

// Protocol delimitation migration plan.
//-----------------------------------------------------------------------------
//    v1/v2 server: ROUTER, requires not delimited
//       v3 server: ROUTER, allows/echos delimited
// v1/v2/v3 client: DEALER (not delimited)
//-----------------------------------------------------------------------------
//       v4 server: ROUTER, requires delimited
//       v4 client: DEALER (delimited) or REQ
//-----------------------------------------------------------------------------

// Convert an error code to data for payload.
data_chunk message::to_bytes(const code& ec)
{
    return build_chunk(
    {
        to_little_endian(static_cast<uint32_t>(ec.value()))
    });
}

// Constructors.
//-----------------------------------------------------------------------------

// Incoming messages pass their route security.
message::message(bool secure)
  : id_(0), secure_(secure)
{
}

message::message(const message& request, const code& ec)
  : message(request, to_bytes(ec))
{
}

message::message(const message& request, data_chunk&& data)
  : command_(request.command_),
    id_(request.id_),
    data_(std::move(data)),
    route_(request.route_),
    secure_(false)
{
}

message::message(const subscription& route, const std::string& command,
    const code& ec)
  : message(route, command, to_bytes(ec))
{
}

message::message(const subscription& route, const std::string& command,
    data_chunk&& data)
  : command_(command),
    id_(route.id()),
    data_(std::move(data)),
    route_(route),
    secure_(false)
{
}

// Properties.
//-----------------------------------------------------------------------------

const std::string& message::command() const
{
    return command_;
}

uint32_t message::id() const
{
    return id_;
}

const data_chunk& message::data() const
{
    return data_;
}

const server::route& message::route() const
{
    return route_;
}

bool message::secure() const
{
    return secure_;
}

// Transport.
//-------------------------------------------------------------------------

code message::receive(zmq::socket& socket)
{
    zmq::message message;
    const auto ec = socket.receive(message);

    if (ec)
        return ec;

    if (message.size() < 4 || message.size() > 5)
        return error::bad_stream;

    // Decode the routing information.
    //-------------------------------------------------------------------------

    // Client is undelimited DEALER -> 1 addresses with 0 delimiter (4), or
    // client is REQ or delimited DEALER -> 1 addresses with 1 delimiter (5).

    zmq::message::address address;

    if (!message.dequeue(address))
        return error::bad_stream;

    route_.set_address(address);

    // In the reply we echo the delimited-ness of the original request.
    // If there are three frames left the message must be undelimited.
    route_.set_delimited(message.size() == 4);

    // Drop the delimiter so that there are always (3) frames remaining.
    if (route_.delimited() && !message.dequeue_data().empty())
        return error::bad_stream;

    // All libbitcoin queries and responses have these three frames.
    //-------------------------------------------------------------------------

    // Query command (returned to caller).
    command_ = message.dequeue_text();

    // Arbitrary caller data (returned to caller for correlation).
    if (!message.dequeue(id_))
        return error::bad_stream;

    // Serialized query.
    data_ = message.dequeue_data();

    return error::success;
}

code message::send(zmq::socket& socket) const
{
    zmq::message message;

    // Encode the routing information.
    //-------------------------------------------------------------------------

    // Client is undelimited DEALER -> 1 address with 0 delimiter (4).
    // Client is REQ or delimited DEALER -> 1 address with 1 delimiter (5).
    message.enqueue(route_.address());

    // In the reply we echo the delimited-ness of the original request.
    if (route_.delimited())
        message.enqueue();

    // All libbitcoin queries and responses have these three frames.
    //-------------------------------------------------------------------------
    message.enqueue(command_);
    message.enqueue_little_endian(id_);
    message.enqueue(data_);

    return socket.send(message);
}

} // namespace server
} // namespace libbitcoin
