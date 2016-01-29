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
#include <czmq++/czmqpp.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/message/incoming.hpp>
#include <bitcoin/server/message/outgoing.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {
    
using std::placeholders::_1;

// TODO: should we be testing zmq_fail in each bind call?
static constexpr int zmq_true = 1;
static constexpr int zmq_false = 0;
static constexpr int zmq_fail = -1;
static constexpr int zmq_curve_enabled = zmq_true;
static constexpr int zmq_socket_no_linger = zmq_false;

const auto now = []()
{
    return boost::posix_time::second_clock::universal_time();
};

receiver::receiver(const settings& settings)
  : counter_(0),
    sender_(context_),
    settings_(settings),
    socket_(context_, ZMQ_ROUTER),
    wakeup_socket_(context_, ZMQ_PULL),
    heartbeat_socket_(context_, ZMQ_PUB),
    authenticate_(context_)
{
    BITCOIN_ASSERT(socket_.self() != nullptr);
    BITCOIN_ASSERT(wakeup_socket_.self() != nullptr);
    BITCOIN_ASSERT(heartbeat_socket_.self() != nullptr);
}

bool receiver::start()
{
#ifdef _MSC_VER
    if (settings_.log_requests)
        log::debug(LOG_SERVICE)
            << "Authentication logging disabled on Windows.";
#else
    // This exposes the log stream to non-utf8 text on Windows.
    // TODO: fix zeromq/czmq/czmqpp to be utf8 everywhere.
    if (settings_.log_requests)
        authenticate_.set_verbose(true);
#endif 

    if (!settings_.whitelists.empty())
        whitelist();

    if (settings_.certificate_file.empty())
        socket_.set_zap_domain("global");

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
    rc = socket_.bind(endpoint);

    if (rc == zmq_false)
    {
        log::error(LOG_SERVICE)
            << "Failed to bind query service on " << endpoint;
        return false;
    }

    socket_.set_linger(zmq_socket_no_linger);
    log::info(LOG_SERVICE)
        << "Bound query service on " << endpoint;

    // This binds the heartbeat service.
    endpoint = settings_.heartbeat_endpoint.to_string();
    rc = heartbeat_socket_.bind(endpoint);

    if (rc == zmq_false)
    {
        log::error(LOG_SERVICE)
            << "Failed to bind heartbeat service on " << endpoint;
        return false;
    }

    deadline_ = now() + settings_.heartbeat_interval();
    log::info(LOG_SERVICE)
        << "Bound heartbeat service on " << endpoint;

    return true;
}

static std::string format_whitelist(const config::authority& authority)
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
    if (settings_.certificate_file.empty())
        return true;

    std::string client_certs(CURVE_ALLOW_ANY);
    if (!settings_.client_certificates_path.empty())
        client_certs = settings_.client_certificates_path.string();

    authenticate_.configure_curve("*", client_certs);
    czmqpp::certificate cert(settings_.certificate_file.string());

    if (cert.valid())
    {
        cert.apply(socket_);
        socket_.set_curve_server(zmq_curve_enabled);
        return true;
    }

    return false;
}

void receiver::attach(const std::string& command,
    command_handler handler)
{
    handlers_[command] = handler;
}

void receiver::poll()
{
    // Poll for network updates.
    czmqpp::poller poller(socket_, wakeup_socket_);
    BITCOIN_ASSERT(poller.self() != nullptr);

    const auto which = poller.wait(settings_.polling_interval_seconds);
    BITCOIN_ASSERT(socket_.self() != nullptr);
    BITCOIN_ASSERT(wakeup_socket_.self() != nullptr);

    if (which == wakeup_socket_)
    {
        // Send queued message.
        czmqpp::message message;
        message.receive(wakeup_socket_);
        message.send(socket_);
    }
    else if (which == socket_)
    {
        // Get message: 6-part envelope + content -> request
        incoming request;
        request.receive(socket_);

        // Perform request if handler exists.
        auto it = handlers_.find(request.command());
        if (it != handlers_.end())
        {
            if (settings_.log_requests)
                log::debug(LOG_REQUEST)
                    << "Service request [" << request.command() << "] from "
                    << encode_base16(request.origin());

            it->second(request,
                std::bind(&sender::queue,
                    &sender_, _1));
        }
        else
        {
            if (settings_.log_requests)
                log::warning(LOG_REQUEST)
                    << "Unhandled service request [" << request.command()
                    << "] from " << encode_base16(request.origin());
        }
    }

    // Publish heartbeat.
    if (now() > deadline_)
    {
        deadline_ = now() + settings_.heartbeat_interval();
        log::debug(LOG_SERVICE) << "Publish service heartbeat";
        publish_heartbeat();
    }
}

void receiver::publish_heartbeat()
{
    czmqpp::message message;
    const auto raw_counter = to_chunk(to_little_endian(counter_));
    message.append(raw_counter);
    message.send(heartbeat_socket_);
    ++counter_;
}

} // namespace server
} // namespace libbitcoin
