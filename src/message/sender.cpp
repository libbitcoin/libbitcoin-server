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

#include <czmq++/czmqpp.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/message/outgoing_message.hpp>

namespace libbitcoin {
namespace server {
    
using std::placeholders::_1;

static constexpr int zmq_fail = -1;

sender::sender(czmqpp::context& context)
  : context_(context)
{
}

void sender::queue(const outgoing_message& message)
{
    czmqpp::socket socket(context_, ZMQ_PUSH);
    BITCOIN_ASSERT(socket.self() != nullptr);

    // Returns 0 if OK, -1 if the endpoint was invalid.
    const auto rc = socket.connect("inproc://trigger-send");

    if (rc == zmq_fail)
    {
        log::error(LOG_SERVICE)
            << "Failed to connect to send queue.";
        return;
    }

    message.send(socket);
    socket.destroy(context_);
}

} // namespace server
} // namespace libbitcoin
