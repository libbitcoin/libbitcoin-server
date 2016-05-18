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
#include <bitcoin/server/message/sender.hpp>

#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/message/outgoing.hpp>

namespace libbitcoin {
namespace server {
    
using std::placeholders::_1;
using namespace bc::protocol;

sender::sender(zmq::context& context)
  : context_(context)
{
}

void sender::queue(const outgoing& message)
{
    zmq::socket socket(context_, zmq::socket::role::pusher);

    if (!socket || !socket.connect("inproc://trigger-send"))
    {
        log::error(LOG_SERVICE)
            << "Failed to connect to send queue.";

        return;
    }

    message.send(socket);
}

} // namespace server
} // namespace libbitcoin
