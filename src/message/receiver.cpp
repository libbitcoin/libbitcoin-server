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
#include <bitcoin/server/message/receiver.hpp>

#include <cstdint>
#include <vector>
#include <boost/date_time.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/message/incoming.hpp>
#include <bitcoin/server/message/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using namespace bc::config;
using namespace bc::protocol;
using namespace boost::posix_time;

receiver::receiver(server_node::ptr node)
  : counter_(0),
    sender_(context_),
    settings_(node->server_settings()),
    wakeup_socket_(context_, zmq::socket::role::puller),
    heartbeat_socket_(context_, zmq::socket::role::publisher),
    receive_socket_(context_, zmq::socket::role::router),
    authenticate_(context_)
{
    BITCOIN_ASSERT((bool)wakeup_socket_);
    BITCOIN_ASSERT((bool)heartbeat_socket_);
    BITCOIN_ASSERT((bool)receive_socket_);

    poller_.add(receive_socket_);
    poller_.add(wakeup_socket_);
}

bool receiver::start()
{
    // The receiver is only enabled for query and notification.
    if (!settings_.queries_enabled && settings_.subscription_limit == 0)
        return true;

    if (!settings_.server_secret_key.empty())
    {
        if (!receive_socket_.set_secret_key(settings_.server_secret_key) ||
            !receive_socket_.set_curve_server())
        {
            log::error(LOG_SERVICE)
                << "Invalid server key.";
            return false;
        }

        log::info(LOG_SERVICE)
            << "Loaded server key.";
    }

    // Client authentication requires a server key.
    if (settings_.server_secret_key.empty() &&
        !settings_.client_public_keys.empty())
    {
        log::error(LOG_SERVICE)
            << "Client authentication requires a server key.";
        return false;
    }

    load_whitelist();

    if (!wakeup_socket_.bind("inproc://trigger-send"))
    {
        log::error(LOG_SERVICE)
            << "Failed to connect to request queue.";
        return false;
    }

    const auto query_endpoint = settings_.query_endpoint.to_string();
    if (!receive_socket_.bind(query_endpoint))
    {
        log::error(LOG_SERVICE)
            << "Failed to bind query service on " << query_endpoint;
        return false;
    }

    log::info(LOG_SERVICE)
        << "Bound query service on " << query_endpoint;

    const auto heartbeat_endpoint = settings_.heartbeat_endpoint.to_string();
    if (!heartbeat_socket_.bind(heartbeat_endpoint))
    {
        log::error(LOG_SERVICE)
            << "Failed to bind heartbeat service on " << heartbeat_endpoint;
        return false;
    }

    update_heartbeat();

    log::info(LOG_SERVICE)
        << "Bound heartbeat service on " << heartbeat_endpoint;

    return true;
}

static std::string format_whitelist(const authority& authority)
{
    auto formatted = authority.to_string();

    if (authority.port() == 0)
        formatted += ":*";

    return formatted;
}

void receiver::load_whitelist()
{
    for (const auto& public_key: settings_.client_public_keys)
    {
        log::info(LOG_SERVICE)
            << "Allowed client [" << public_key << "]";

        authenticate_.allow(public_key);
    }

    for (const auto& address : settings_.client_addresses)
    {
        log::info(LOG_SERVICE)
            << "Allowed client [" << format_whitelist(address) << "]";

        authenticate_.allow(address.to_string());
    }
}

void receiver::attach(const std::string& command, command_handler handler)
{
    handlers_[command] = handler;
}

ptime receiver::now()
{
    return second_clock::universal_time();
};

void receiver::update_heartbeat()
{
    heartbeat_ = now() + settings_.heartbeat_interval();
}

void receiver::poll(uint32_t interval_microseconds)
{
    // Poller granularity limits the heartbeat interval.
    publish_heartbeat();

    // The socket id can be zero (none) or a registered socket id.
    const auto socket_id = poller_.wait(interval_microseconds);

    if (socket_id == receive_socket_.id())
    {
        // Get message: 6-part envelope + content -> request
        incoming request;
        request.receive(receive_socket_);

        if (settings_.log_requests)
            log::info(LOG_REQUEST)
                << "Command " << request.command() << " from "
                << encode_base16(request.origin());

        // Locate the request handler for this command.
        auto handler = handlers_.find(request.command());

        // Perform request if handler exists.
        if (handler != handlers_.end())
            handler->second(request,
                std::bind(&sender::queue,
                    &sender_, _1));
    }
    else if (socket_id == wakeup_socket_.id())
    {
        // Send queued message.
        zmq::message message;
        message.receive(wakeup_socket_);
        message.send(receive_socket_);
    }
}

void receiver::publish_heartbeat()
{
    if (now() > heartbeat_)
    {
        update_heartbeat();
        zmq::message message;
        const auto raw_counter = to_chunk(to_little_endian(counter_));
        message.append(raw_counter);
        message.send(heartbeat_socket_);
        ++counter_;

        log::debug(LOG_SERVICE)
            << "Published service heartbeat.";
    }
}

} // namespace server
} // namespace libbitcoin
