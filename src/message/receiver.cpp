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

static constexpr int zmq_fail = -1;
static constexpr int zmq_unbound = 0;

receiver::receiver(server_node::ptr node)
  : counter_(0),
    sender_(context_),
    settings_(node->server_settings()),
    wakeup_socket_(context_, ZMQ_PULL),
    heartbeat_socket_(context_, ZMQ_PUB),
    receive_socket_(context_, ZMQ_ROUTER),
    authenticate_(context_),
    poller_(receive_socket_, wakeup_socket_)
{
    BITCOIN_ASSERT(poller_.self() != nullptr);
    BITCOIN_ASSERT(wakeup_socket_.self() != nullptr);
    BITCOIN_ASSERT(heartbeat_socket_.self() != nullptr);
    BITCOIN_ASSERT(receive_socket_.self() != nullptr);
}

bool receiver::start()
{
    // The receiver is only enabled for query and notification.
    if (!settings_.queries_enabled && settings_.subscription_limit == 0)
        return true;

    if (settings_.log_requests)
        authenticate_.set_verbose(true);

    if (!settings_.whitelists.empty())
        whitelist();

    if (settings_.certificate_file.empty())
        receive_socket_.set_zap_domain("global");

    if (!enable_crypto())
    {
        log::error(LOG_SERVICE)
            << "Invalid server certificate.";
        return false;
    }

    // This binds the request queue.
    // Returns 0 if OK, -1 if the endpoint was invalid.
    auto rc = wakeup_socket_.bind("inproc://trigger-send");

    if (rc == zmq_fail)
    {
        log::error(LOG_SERVICE)
            << "Failed to connect to request queue.";
        return false;
    }

    // This binds the query service.
    auto endpoint = settings_.query_endpoint.to_string();
    rc = receive_socket_.bind(endpoint);

    if (rc == zmq_unbound)
    {
        log::error(LOG_SERVICE)
            << "Failed to bind query service on " << endpoint;
        return false;
    }

    log::info(LOG_SERVICE)
        << "Bound query service on " << endpoint;

    // This binds the heartbeat service.
    endpoint = settings_.heartbeat_endpoint.to_string();
    rc = heartbeat_socket_.bind(endpoint);

    if (rc == zmq_unbound)
    {
        log::error(LOG_SERVICE)
            << "Failed to bind heartbeat service on " << endpoint;
        return false;
    }

    update_heartbeat();

    log::info(LOG_SERVICE)
        << "Bound heartbeat service on " << endpoint;

    return true;
}

static std::string format_whitelist(const authority& authority)
{
    auto formatted = authority.to_string();

    if (authority.port() == 0)
        formatted += ":*";

    return formatted;
}

void receiver::whitelist()
{
    for (const auto& ip_address: settings_.whitelists)
    {
        log::info(LOG_SERVICE)
            << "Whitelisted client [" << format_whitelist(ip_address) << "]";

        authenticate_.allow(ip_address.to_string());
    }
}

// Returns false if server certificate exists and is invalid.
bool receiver::enable_crypto()
{
    const auto server_cert_path = settings_.certificate_file.string();
    auto client_certs_path = settings_.client_certificates_path.string();

    if (!client_certs_path.empty() && server_cert_path.empty())
    {
        log::error(LOG_SERVICE)
            << "Client authentication requires a server certificate.";

        return false;
    }

    // Configure server certificate if specified.
    if (!server_cert_path.empty())
    {
        certificate_.reset(server_cert_path);

        if (!certificate_)
            return false;

        certificate_.apply(receive_socket_);
        receive_socket_.set_curve_server();

        log::info(LOG_SERVICE)
            << "Loaded server certificate: " << server_cert_path;
    }

    // Always configure client certificates directory or *.
    if (client_certs_path.empty())
        client_certs_path = CURVE_ALLOW_ANY;
    else
        log::info(LOG_SERVICE)
            << "Require client certificates: " << client_certs_path;

    static const auto all_domains = "*";
    authenticate_.configure_curve(all_domains, client_certs_path);
    return true;
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

void receiver::poll(uint32_t interval_milliseconds)
{
    // Poller granularity limits the heartbeat interval.
    publish_heartbeat();

    const auto socket = poller_.wait(interval_milliseconds);

    if (socket == receive_socket_)
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
    else if (socket == wakeup_socket_)
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
