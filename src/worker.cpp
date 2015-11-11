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
#include <bitcoin/server/worker.hpp>

#include <cstdint>
#include <vector>
#include <boost/date_time.hpp>
#include <czmq++/czmqpp.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/config/configuration.hpp>
#include <bitcoin/server/config/settings.hpp>

namespace libbitcoin {
namespace server {
    
using std::placeholders::_1;

constexpr int zmq_true = 1;
constexpr int zmq_false = 0;
constexpr int zmq_fail = -1;
constexpr int zmq_curve_enabled = zmq_true;
constexpr int zmq_socket_no_linger = zmq_false;

const auto now = []()
{
    return boost::posix_time::second_clock::universal_time();
};

send_worker::send_worker(czmqpp::context& context)
  : context_(context)
{
}

void send_worker::queue_send(const outgoing_message& message)
{
    czmqpp::socket socket(context_, ZMQ_PUSH);
    BITCOIN_ASSERT(socket.self() != nullptr);

    // Returns 0 if OK, -1 if the endpoint was invalid.
    int rc = socket.connect("inproc://trigger-send");
    if (rc == zmq_fail)
    {
        log::error(LOG_SERVICE)
            << "Failed to connect to send queue.";
        return;
    }

    message.send(socket);
    socket.destroy(context_);
}

request_worker::request_worker(const settings& settings)
  : socket_(context_, ZMQ_ROUTER),
    wakeup_socket_(context_, ZMQ_PULL),
    heartbeat_socket_(context_, ZMQ_PUB),
    authenticate_(context_),
    sender_(context_),
    settings_(settings)
{
    BITCOIN_ASSERT(socket_.self() != nullptr);
    BITCOIN_ASSERT(wakeup_socket_.self() != nullptr);
    BITCOIN_ASSERT(heartbeat_socket_.self() != nullptr);

    // Returns 0 if OK, -1 if the endpoint was invalid.
    int rc = wakeup_socket_.bind("inproc://trigger-send");

    if (rc == zmq_fail)
        log::error(LOG_SERVICE)
            << "Failed to connect to request queue.";
}

bool request_worker::start()
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

    // This binds the query service.
    if (!create_new_socket())
    {
        log::error(LOG_SERVICE)
            << "Failed to bind query service on "
            << settings_.query_endpoint;
        return false;
    }

    log::info(LOG_SERVICE)
        << "Bound query service on "
        << settings_.query_endpoint;

    // This binds the heartbeat service.
    const auto rc = heartbeat_socket_.bind(
        settings_.heartbeat_endpoint.to_string());
    if (rc == 0)
    {
        log::error(LOG_SERVICE)
            << "Failed to bind heartbeat service on "
            << settings_.heartbeat_endpoint;
        return false;
    }

    log::info(LOG_SERVICE)
        << "Bound heartbeat service on "
        << settings_.heartbeat_endpoint;

    deadline_ = now() + settings_.heartbeat_interval();
    return true;
}

bool request_worker::stop()
{
    return true;
}

static std::string format_whitelist(const config::authority& authority)
{
    auto formatted = authority.to_string();
    if (authority.port() == 0)
        formatted += ":*";

    return formatted;
}

void request_worker::whitelist()
{
    for (const auto& ip_address : settings_.whitelists)
    {
        log::info(LOG_SERVICE)
            << "Whitelisted client [" << format_whitelist(ip_address) << "]";
        authenticate_.allow(ip_address.to_string());
    }
}

bool request_worker::enable_crypto()
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

bool request_worker::create_new_socket()
{
    // Not sure what we would use this for, so disabled for now.
    // Set the socket identity name.
    // socket_.set_identity(...);

    // Connect...
    const auto rc = socket_.bind(settings_.query_endpoint.to_string());
    const auto connected = rc != 0;

    if (connected)
        socket_.set_linger(zmq_socket_no_linger);

    return connected;
}

void request_worker::attach(const std::string& command,
    command_handler handler)
{
    handlers_[command] = handler;
}

void request_worker::update()
{
    poll();
}

void request_worker::poll()
{
    // Poll for network updates.
    czmqpp::poller poller(socket_, wakeup_socket_);
    BITCOIN_ASSERT(poller.self() != nullptr);

    const auto which = poller.wait(settings_.polling_interval_seconds);
    BITCOIN_ASSERT(socket_.self() != nullptr);
    BITCOIN_ASSERT(wakeup_socket_.self() != nullptr);

    if (which == socket_)
    {
        // Get message: 6-part envelope + content -> request
        incoming_message request;
        request.recv(socket_);

        // Perform request if handler exists.
        auto it = handlers_.find(request.command());
        if (it != handlers_.end())
        {
            if (settings_.log_requests)
                log::debug(LOG_REQUEST)
                    << "Service request [" << request.command() << "] from "
                    << encode_base16(request.origin());

            it->second(request,
                std::bind(&send_worker::queue_send,
                    &sender_, _1));
        }
        else
        {
            log::warning(LOG_SERVICE)
                << "Unhandled service request [" << request.command()
                << "] from " << encode_base16(request.origin());
        }
    }
    else if (which == wakeup_socket_)
    {
        // Send queued message.
        czmqpp::message message;
        message.receive(wakeup_socket_);
        message.send(socket_);
    }

    // Publish heartbeat.
    if (now() > deadline_)
    {
        deadline_ = now() + settings_.heartbeat_interval();
        log::debug(LOG_SERVICE) << "Publish service heartbeat";
        publish_heartbeat();
    }
}

void request_worker::publish_heartbeat()
{
    static uint32_t counter = 0;
    czmqpp::message message;
    const auto raw_counter = to_chunk(to_little_endian(counter));
    message.append(raw_counter);
    message.send(heartbeat_socket_);
    ++counter;
}

} // namespace server
} // namespace libbitcoin
