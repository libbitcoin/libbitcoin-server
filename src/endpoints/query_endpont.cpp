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

namespace libbitcoin {
namespace server {

#define NAME "query_endpoint"

using std::placeholders::_1;
using namespace bc::config;
using namespace bc::protocol;
using namespace boost::posix_time;

// Arbitrary inprocess endpoint for outbound queueing.
static const auto queue_endpoint = "inproc://trigger-send";

query_endpoint::query_endpoint(zmq::context::ptr context, server_node* node)
  : stopped_(true),
    dispatch_(node->thread_pool(), NAME),
    settings_(node->server_settings()),
    query_socket_(*context, zmq::socket::role::router),
    pull_socket_(context_, zmq::socket::role::puller),
    push_socket_(context_, zmq::socket::role::pusher)
{
    BITCOIN_ASSERT(push_socket_);
    BITCOIN_ASSERT(pull_socket_);
    BITCOIN_ASSERT(query_socket_);

    poller_.add(pull_socket_);
    poller_.add(query_socket_);
}

bool query_endpoint::start()
{
    if (!settings_.query_endpoint_enabled)
        return true;

    if (!pull_socket_.bind(queue_endpoint))
    {
        log::error(LOG_SERVICE)
            << "Failed to create to send queue.";
        return false;
    }

    if (!push_socket_.connect(queue_endpoint))
    {
        log::error(LOG_SERVICE)
            << "Failed to connect to send queue.";
        return false;
    }

    const auto query_endpoint = settings_.query_endpoint.to_string();

    if (!query_socket_.set_authentication_domain("query") ||
        !query_socket_.bind(query_endpoint))
    {
        log::error(LOG_SERVICE)
            << "Failed to bind query service on " << query_endpoint;
        return false;
    }

    log::info(LOG_SERVICE)
        << "Bound query service on " << query_endpoint;

    // TODO: start polling thread here.
    // dispatch_.concurrent();
    stopped_.store(false);
    return true;
}

void query_endpoint::stop()
{
    stopped_.store(true);
}

// TODO: use this to stop the internal context after the external stops.
bool query_endpoint::stopped() const
{
    return stopped_.load();
}

void query_endpoint::attach(const std::string& command,
    command_handler handler)
{
    handlers_[command] = handler;
}

void query_endpoint::poll(uint32_t interval_milliseconds)
{
    // The socket id can be zero (none) or a registered socket id.
    const auto socket_id = poller_.wait(interval_milliseconds);

    if (socket_id == query_socket_.id())
    {
        incoming request;

        if (!request.receive(query_socket_))
        {
            log::info(LOG_REQUEST)
                << "Malformed request from "
                << encode_base16(request.address);
            return;
        }

        if (settings_.log_requests)
        {
            log::info(LOG_REQUEST)
                << "Command " << request.command << " from "
                << encode_base16(request.address);
        }

        // Locate the request handler for this command.
        const auto handler = handlers_.find(request.command);

        // Perform request if handler exists.
        if (handler == handlers_.end())
        {
            log::info(LOG_REQUEST)
                << "Invalid command from "
                << encode_base16(request.address);
        }

        // Push an outgoing message.
        handler->second(request,
            std::bind(&query_endpoint::enqueue,
                shared_from_this(), _1));
    }
    else if (socket_id == pull_socket_.id())
    {
        // Pull and send an outgoing message.
        dequeue();
    }
}

// It's not clear what is the benefit of the queue. The poll is still blocked
// when we are delayed in transmitting the message, just as it would be if we
// simply did not enqueue the message and instead sent it directly. We should
// either move the queue onto another thread or eliminate it altogether.
void query_endpoint::dequeue()
{
    zmq::message message;
    /* bool */ message.receive(pull_socket_);
    /* bool */ message.send(query_socket_);
}

void query_endpoint::enqueue(outgoing& message)
{
    /* bool */ message.send(push_socket_);
}

} // namespace server
} // namespace libbitcoin
