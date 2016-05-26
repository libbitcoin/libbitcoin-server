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
#include <bitcoin/server/endpoints/query_endpoint.hpp>

#include <cstdint>
#include <vector>
#include <boost/date_time.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define NAME "query_endpoint"

using std::placeholders::_1;
using namespace bc::config;
using namespace bc::protocol;
using namespace boost::posix_time;

// Arbitrary inprocess endpoint for outbound queueing.
static const auto queue_endpoint = "inproc://trigger-send";

static constexpr uint32_t polling_interval_milliseconds = 1;

query_endpoint::query_endpoint(curve_authenticator& authenticator,
    server_node* node)
  : dispatch_(node->thread_pool(), NAME),
    settings_(node->server_settings()),
    socket_(authenticator, zmq::socket::role::router),
    pull_socket_(context_, zmq::socket::role::puller),
    push_socket_(context_, zmq::socket::role::pusher)
{
    if (!settings_.query_endpoint_enabled)
        return;

    if (!authenticator.apply(socket_, NAME))
        socket_.stop();
}

// Start.
//-----------------------------------------------------------------------------

bool query_endpoint::start()
{
    if (!settings_.query_endpoint_enabled)
    {
        stop();
        return true;
    }

    if (!start_queue() || !start_endpoint())
    {
        stop();
        return false;
    }

    return true;
}

bool query_endpoint::start_queue()
{
    if (!pull_socket_ || !pull_socket_.bind(queue_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize to queue puller.";
        return false;
    }

    if (!push_socket_ || !push_socket_.connect(queue_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize to queue pusher.";
        return false;
    }

    poller_.add(pull_socket_);
    return true;
}

bool query_endpoint::start_endpoint()
{
    const auto query_endpoint = settings_.query_endpoint.to_string();

    if (!socket_ || !socket_.bind(query_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize query service on " << query_endpoint;
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound query service on " << query_endpoint;

    dispatch_.concurrent(
        std::bind(&query_endpoint::monitor,
            this));

    poller_.add(socket_);
    return true;
}

// Stop.
//-----------------------------------------------------------------------------
 
bool query_endpoint::stop()
{
    const auto stopped = push_socket_.stop() && pull_socket_.stop() && 
        socket_.stop();

    if (stopped)
        poller_.clear();

    return stopped;
}

// Monitors.
//-----------------------------------------------------------------------------

void query_endpoint::monitor()
{
    // Ignore expired and keep looping, exiting the thread when terminated.
    while (!poller_.terminated())
    {
        const auto socket_id = poller_.wait(polling_interval_milliseconds);

        if (socket_id == socket_.id())
            receive();
        else if (socket_id == pull_socket_.id())
            dequeue();
    }

    // When context is stopped the loop terminates, still need to stop sockets.
    stop();
}

// Utilities.
//-----------------------------------------------------------------------------

void query_endpoint::receive()
{
    incoming request;

    if (!request.receive(socket_))
    {
        log::info(LOG_ENDPOINT)
            << "Malformed request from "
            << encode_base16(request.address);
        return;
    }

    if (settings_.log_requests)
    {
        log::info(LOG_ENDPOINT)
            << "Command " << request.command << " from "
            << encode_base16(request.address);
    }

    // Locate the request handler for this command.
    const auto handler = handlers_.find(request.command);

    if (handler == handlers_.end())
    {
        log::info(LOG_ENDPOINT)
            << "Invalid command from "
            << encode_base16(request.address);
    }

    // Execute the request if a handler exists.
    handler->second(request,
        std::bind(&query_endpoint::enqueue,
            this, _1));
}

void query_endpoint::dequeue()
{
    zmq::message message;
    if (message.receive(pull_socket_) && message.send(socket_))
        return;

    log::debug(LOG_ENDPOINT)
        << "Failed to dequeue message.";
}

void query_endpoint::enqueue(outgoing& message)
{
    if (message.send(push_socket_))
        return;

    log::debug(LOG_ENDPOINT)
        << "Failed to enqueue message.";
}

void query_endpoint::attach(const std::string& command,
    command_handler handler)
{
    handlers_[command] = handler;
}

} // namespace server
} // namespace libbitcoin
