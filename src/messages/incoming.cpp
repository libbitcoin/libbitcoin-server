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

bool incoming::receive(zmq::socket& socket)
{
    zmq::message message;

    // BUGBUG: this supports only single hop routing (zero or one address).
    if (!message.receive(socket) || message.size() < 3 || message.size() > 5)
        return false;

    if (message.size() == 5)
    {
        // Delimited requests are new in v3 and backward compatible.
        // REQ adds addressing frame and delimiter.
        address = message.dequeue_data();
        message.dequeue();
    }
    else if (message.size() == 4)
    {
        // TODO: our client dealer should prepend the delimiter frame.
        // See: zeromq.org/tutorials:dealer-and-router
        // DEALER adds addressing frame only.
        address = message.dequeue_data();
    }

    command = message.dequeue_text();

    if (!message.dequeue(id))
        return false;

    // This copy could be avoided by deferring the dequeue.
    data = message.dequeue_data();
    return true;
}

} // namespace server
} // namespace libbitcoin
