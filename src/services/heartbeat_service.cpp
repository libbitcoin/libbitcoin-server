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
#include <bitcoin/server/services/heartbeat_service.hpp>

#include <algorithm>
#include <cstdint>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

static const auto domain = "heartbeat";

using namespace bc::config;
using namespace bc::protocol;
using role = zmq::socket::role;

// Heartbeat is capped at ~ 25 days by signed/millsecond conversions.
heartbeat_service::heartbeat_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    security_(secure ? "secure" : "public"),
    settings_(node.server_settings()),
    external_(node.protocol_settings()),
    service_(settings_.heartbeat_endpoint(secure)),
    authenticator_(authenticator),
    node_(node),

    // Pick a random sequence counter start, will wrap around at overflow.
    sequence_(static_cast<uint16_t>(pseudo_random(0, max_uint16)))
{
}

// Implement service as a publisher.
// The publisher drops messages for lost peers (clients) and high water.
void heartbeat_service::work()
{
    zmq::socket publisher(authenticator_, role::publisher, external_);

    // Bind socket to the service endpoint.
    if (!started(bind(publisher)))
        return;

    const auto period = pulse_milliseconds();
    zmq::poller poller;
    poller.add(publisher);

    // TODO: make member, see tx/block publishers.
    // BUGBUG: stop is insufficient to stop worker, because of long period.
    // We will not receive on the poller, we use its timer and context stop.
    while (!poller.terminated() && !stopped())
    {
        poller.wait(period);
        publish(publisher);
    }

    // Unbind the socket and exit this thread.
    finished(unbind(publisher));
}

int32_t heartbeat_service::pulse_milliseconds() const
{
    const int64_t seconds = settings_.heartbeat_service_seconds;
    const int64_t milliseconds = seconds * 1000;
    auto capped = std::min(milliseconds, static_cast<int64_t>(max_int32));
    return static_cast<int32_t>(capped);
}

// Bind/Unbind.
//-----------------------------------------------------------------------------

bool heartbeat_service::bind(zmq::socket& publisher)
{
    if (!authenticator_.apply(publisher, domain, secure_))
        return false;

    const auto ec = publisher.bind(service_);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " heartbeat service to "
            << service_ << " : " << ec.message();
        return false;
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " heartbeat service to " << service_;
    return true;
}

bool heartbeat_service::unbind(zmq::socket& publisher)
{
    // Don't log stop success.
    if (publisher.stop())
        return true;

    LOG_ERROR(LOG_SERVER)
        << "Failed to disconnect " << security_ << " heartbeat worker.";
    return false;
}

// Publish Execution (integral worker).
//-----------------------------------------------------------------------------

void heartbeat_service::publish(zmq::socket& publisher)
{
    if (stopped())
        return;

    // [ sequence:2 ]
    // [ height:4 ]
    zmq::message message;
    message.enqueue_little_endian(++sequence_);
    message.enqueue_little_endian(node_.top_block().height());

    auto ec = publisher.send(message);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failed to publish " << security_ << " heartbeat: "
            << ec.message();
        return;
    }

    // This isn't actually a request, should probably update settings.
    LOG_VERBOSE(LOG_SERVER)
        << "Published " << security_ << " heartbeat [" << sequence_ << "].";
}

} // namespace server
} // namespace libbitcoin
