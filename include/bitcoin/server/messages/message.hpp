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
#ifndef LIBBITCOIN_SERVER_MESSAGE
#define LIBBITCOIN_SERVER_MESSAGE

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/route.hpp>
#include <bitcoin/server/messages/subscription.hpp>

namespace libbitcoin {
namespace server {

class BCS_API message
{
public:
    static data_chunk to_bytes(const code& ec);

    // Constructors.
    //-------------------------------------------------------------------------

    /// Construct a default message (to be read).
    message(bool secure);

    // Create an error message in respose to the request.
    message(const message& request, const code& ec);

    // Create a general message in respose to the request.
    message(const message& request, data_chunk&& data);

    // Create an error notification message for the subscription.
    message(const subscription& route, const std::string& command,
        const code& ec);

    //// Construct a notification message for the subscription.
    message(const subscription& route, const std::string& command,
        data_chunk&& data);

    // Properties.
    //-------------------------------------------------------------------------

    /// Query command (used for subscription, always returned to caller).
    const std::string& command() const;

    /// Arbitrary caller data (returned to caller for correlation).
    uint32_t id() const;

    /// Serialized query or response (defined in relation to command).
    const data_chunk& data() const;

    /// The message route.
    const server::route& route() const;

    /// The incoming message route security context.
    bool secure() const;

    // Send/Receive.
    //-------------------------------------------------------------------------

    /// Receive a message via the socket.
    code receive(bc::protocol::zmq::socket& socket);

    /// Send the message via the socket.
    code send(bc::protocol::zmq::socket& socket) const;

protected:
    std::string command_;
    uint32_t id_;
    data_chunk data_;
    server::route route_;
    const bool secure_;
};

typedef std::function<void(const message&)> send_handler;

} // namespace server
} // namespace libbitcoin

#endif
