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
#include <bitcoin/server/web/heartbeat_socket.hpp>

#include <algorithm>
#include <cstdint>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

static const auto domain = "heartbeat";
static constexpr auto poll_interval_milliseconds = 100u;

using namespace bc::config;
using namespace bc::protocol;
using role = zmq::socket::role;

heartbeat_socket::heartbeat_socket(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : manager(authenticator, node, secure, domain)
{
}

void heartbeat_socket::work()
{
    zmq::socket sub(authenticator_, role::subscriber, protocol_settings_);

    const auto endpoint = retrieve_zeromq_connect_endpoint();
    const auto ec = sub.connect(endpoint);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect to heartbeat service " << endpoint << ": "
            << ec.message();
        return;
    }

    if (!started(start_websocket_handler()))
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to start " << security_ << " " << domain
            << " websocket handler";
        return;
    }

    zmq::poller poller;
    poller.add(sub);

    while (!poller.terminated() && !stopped())
    {
        if (poller.wait(poll_interval_milliseconds).contains(sub.id()) &&
            !handle_heartbeat(sub))
            break;
    }

    const auto websocket_stop = stop_websocket_handler();
    const auto sub_stop = sub.stop();

    if (!websocket_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to stop " << security_ << " " << domain
            << " websocket handler: " << ec.message();

    if (!sub_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_
            << " hearbeat websocket service.";

    finished(websocket_stop && sub_stop);
}

const config::endpoint& heartbeat_socket::retrieve_zeromq_endpoint() const
{
    return server_settings_.zeromq_heartbeat_endpoint(secure_);
}

const config::endpoint& heartbeat_socket::retrieve_websocket_endpoint() const
{
    return server_settings_.websockets_heartbeat_endpoint(secure_);
}

} // namespace server
} // namespace libbitcoin
