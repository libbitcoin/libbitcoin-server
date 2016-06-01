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
#include <bitcoin/server/endpoints/heart_endpoint.hpp>

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <memory>
#include <thread>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {
    
#define NAME "heartbeat_endpoint"
#define PUBLIC_NAME "public_heartbeat"
#define SECURE_NAME "secure_heartbeat"

using std::placeholders::_1;
using namespace std::chrono;
using namespace std::this_thread;
using namespace bc::protocol;

static inline bool is_enabled(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return settings.heartbeat_endpoints_enabled &&
        settings.heartbeat_interval_seconds > 0 &&
        (!secure || settings.server_private_key);
}

static inline config::endpoint get_endpoint(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return secure ? settings.secure_heartbeat_endpoint :
        settings.public_heartbeat_endpoint;
}

static inline seconds get_interval(server_node& node)
{
    return seconds(node.server_settings().heartbeat_interval_seconds);
}

// ZMQ_PUB (ok to drop, never block)
// When a ZMQ_PUB socket enters an exceptional state due to having reached
// the high water mark for a subscriber, then any messages that would be sent
// to the subscriber in question shall instead be dropped until the exceptional
// state ends. The zmq_send() function shall never block for this socket type.
heart_endpoint::heart_endpoint(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : authenticator_(authenticator),
    dispatch_(node.thread_pool(), NAME),
    stopped_(true),
    endpoint_(get_endpoint(node, secure)),
    interval_(get_interval(node)),
    log_(node.server_settings().log_requests),
    enabled_(is_enabled(node, secure)),
    secure_(secure)
{
}

// The endpoint is restartable.
bool heart_endpoint::start()
{
    if (!enabled_)
        return true;

    std::promise<code> started;
    code ec(error::operation_failed);

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock();

    if (stopped_)
    {
        // Enable publisher start.
        stopped_ = false;

        // Create the pubisher thread and socket and start sending.
        dispatch_.concurrent(
            std::bind(&heart_endpoint::publisher,
                this, std::ref(started)));

        // Wait on publisher start.
        ec = started.get_future().get();
    }

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (ec)
    {
        log::error(LOG_SERVER)
            << "Failed to bind heartbeat service to " << endpoint_;
        return false;
    }

    log::info(LOG_SERVER)
        << "Bound " << (secure_ ? "secure " : "public ")
        << "heartbeat service to " << endpoint_;
    return true;
}

bool heart_endpoint::stop()
{
    code ec(error::success);

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock();

    if (!stopped_)
    {
        // Stop the publisher.
        stopped_ = true;

        // Wait on publisher stop.
        ec = stopping_.get_future().get();

        // Enable restart capability.
        stopping_ = std::promise<code>();
    }
    
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    if (ec)
    {
        log::error(LOG_SERVER)
            << "Failed to unbind heartbeat service from " << endpoint_;
        return false;
    }

    return true;
}

// A context stop does not stop the publisher.
void heart_endpoint::publisher(std::promise<code>& started)
{
    const auto name = secure_ ? SECURE_NAME : PUBLIC_NAME;
    zmq::socket socket(authenticator_, zmq::socket::role::publisher);

    if (!socket || !authenticator_.apply(socket, name, secure_) ||
        !socket.bind(endpoint_))
    {
        stopping_.set_value(error::success);
        started.set_value(error::operation_failed);
        return;
    }

    started.set_value(error::success);

    // Pick a random counter start, overflow is okay.
    auto counter = static_cast<uint32_t>(pseudo_random());

    // A simple loop is optimal and keeps us on a single thread.
    while (!stopped_)
    {
        send(counter++, socket);
        sleep_for(interval_);
    }

    auto result = socket.stop() ? error::success : error::operation_failed;
    stopping_.set_value(result);
}

void heart_endpoint::send(uint32_t count, zmq::socket& socket)
{
    zmq::message message;
    message.enqueue_little_endian(count);
    const auto result = message.send(socket);

    if (!result)
    {
        log::warning(LOG_SERVER)
            << "Failed to send heartbeat on " << endpoint_;
        return;
    }

    if (log_)
        log::debug(LOG_SERVER)
            << "Heartbeat [" << count << "] sent on " << endpoint_;
}

} // namespace server
} // namespace libbitcoin
