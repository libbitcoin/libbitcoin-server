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
#include <bitcoin/server/web/query_socket.hpp>

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::config;
using namespace bc::protocol;
using role = zmq::socket::role;

static const auto domain = "query";
static constexpr auto poll_interval_milliseconds = 100u;

query_socket::query_socket(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : manager(authenticator, node, secure, domain)
{
}

void query_socket::work()
{
    zmq::socket dealer(authenticator_, role::dealer, internal_);
    zmq::socket query_receiver(authenticator_, role::pair, internal_);

    const auto endpoint = retrieve_connect_endpoint(true);
    auto ec = query_receiver.bind(retrieve_endpoint(true, true));

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " query internal socket: "
            << ec.message();
        return;
    }

    ec = dealer.connect(endpoint);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect to " << security_ << " query socket "
            << endpoint << ": " << ec.message();
        return;
    }

    if (!started(start_websocket_handler()))
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to start " << security_ << " websocket handler: "
            << ec.message();
        return;
    }

    zmq::poller poller;
    poller.add(dealer);
    poller.add(query_receiver);

    while (!poller.terminated() && !stopped())
    {
        const auto identifiers = poller.wait(poll_interval_milliseconds);

        if (identifiers.contains(query_receiver.id()) &&
            !forward(query_receiver, dealer))
            break;

        if (identifiers.contains(dealer.id()) && !handle_query(dealer))
            break;
    }

    const auto websocket_stop = stop_websocket_handler();
    const auto query_stop = query_receiver.stop();
    const auto dealer_stop = dealer.stop();

    if (!websocket_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to stop " << security_ << " websocket handler: "
            << ec.message();

    if (!query_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " websocket query service.";

    if (!dealer_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query connection.";

    finished(websocket_stop && query_stop && dealer_stop);
}

} // namespace server
} // namespace libbitcoin
