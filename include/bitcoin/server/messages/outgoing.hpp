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
#ifndef LIBBITCOIN_SERVER_OUTGOING
#define LIBBITCOIN_SERVER_OUTGOING

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>

namespace libbitcoin {
namespace server {

class BCS_API outgoing
{
public:
    /// Return an error code in response to the incoming query.
    outgoing(const incoming& request, const code& ec);

    /// Return data in response to a successfully-executed incoming query.
    outgoing(const incoming& request, const data_chunk& data);

    /// Return data as a subscription by the given address.
    outgoing(const std::string& command, const data_chunk& data,
        const data_chunk& address1, const data_chunk& address2,
        bool delimited);

    /// A printable address for logging only.
    std::string address();

    /// Send the message one the socket.
    code send(bc::protocol::zmq::socket& socket);

protected:
    outgoing(const std::string& command, const data_chunk& data,
        const data_chunk& address1, const data_chunk& address2, bool delimited,
        uint32_t id);

private:
    bc::protocol::zmq::message message_;
};

typedef std::function<void(outgoing&&)> send_handler;

} // namespace server
} // namespace libbitcoin

#endif
