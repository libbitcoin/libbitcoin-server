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
#include <czmq++/czmqpp.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/config/settings_type.hpp>

namespace libbitcoin {
namespace server {

namespace posix_time = boost::posix_time;
using posix_time::milliseconds;
using posix_time::seconds;
using posix_time::microsec_clock;
using std::placeholders::_1;

constexpr int zmq_true = 1;
constexpr int zmq_false = 0;
constexpr int zmq_fail = -1;
constexpr int zmq_curve_enabled = zmq_true;
constexpr int zmq_socket_no_linger = zmq_false;

auto now = []() { return microsec_clock::universal_time(); };

send_worker::send_worker(czmqpp::context& context)
  : context_(context)
{
    // Disable czmq signal handling.
    zsys_handler_set(NULL);

#ifdef _MSC_VER
    // Hack to prevent czmq from writing to stdout/stderr on Windows.
    // It is necessary to prevent stdio when using our utf8-everywhere pattern.
    // TODO: provide a FILE* here that we can direct to our own log/console.
    zsys_set_logstream(NULL);
#endif
}

void send_worker::queue_send(const outgoing_message& message)
{
    czmqpp::socket socket(context_, ZMQ_PUSH);
    BITCOIN_ASSERT(socket.self() != nullptr);

    // Returns 0 if OK, -1 if the endpoint was invalid.
    int rc = socket.connect("inproc://trigger-send");
    if (rc == zmq_fail)
    {
        log_error(LOG_SERVICE)
            << "Failed to connect to send queue.";
        return;
    }

    message.send(socket);
    socket.destroy(context_);
}

request_worker::request_worker(bool log_requests, 
    uint32_t heartbeat_interval_seconds,
    uint32_t polling_interval_milliseconds)
  : socket_(context_, ZMQ_ROUTER),
    wakeup_socket_(context_, ZMQ_PULL),
    heartbeat_socket_(context_, ZMQ_PUB),
    authenticate_(context_),
    sender_(context_),
    log_requests_(log_requests),
    heartbeat_interval_(heartbeat_interval_seconds),
    polling_interval_milliseconds_(polling_interval_milliseconds)
{
    BITCOIN_ASSERT(socket_.self() != nullptr);
    BITCOIN_ASSERT(wakeup_socket_.self() != nullptr);
    BITCOIN_ASSERT(heartbeat_socket_.self() != nullptr);

    // Returns 0 if OK, -1 if the endpoint was invalid.
    int rc = wakeup_socket_.bind("inproc://trigger-send");
    if (rc == zmq_fail)
    {
        log_error(LOG_SERVICE)
            << "Failed to connect to request queue.";
        return;
    }
}

bool request_worker::start(const settings_type& config)
{
    log_requests_ = config.server.log_requests;

#ifndef _MSC_VER
    // Hack to prevent czmq from writing to stdout/stderr on Windows.
    // This will prevent authentication feedback, but also prevent crashes.
    // It is necessary to prevent stdio when using our utf8-everywhere pattern.
    // TODO: modify czmq to not hardcode stdout/stderr for verbose output.
    if (log_requests_)
        authenticate_.set_verbose(true);
#endif

    if (!config.server.whitelists.empty())
        whitelist(config.server.whitelists);

    if (config.server.certificate_file.empty())
        socket_.set_zap_domain("global");

    if (!enable_crypto(config))
    {
        log_error(LOG_SERVICE)
            << "Invalid server certificate.";
        return false;
    }

    // This binds the query service.
    if (!create_new_socket(config))
    {
        log_error(LOG_SERVICE)
            << "Failed to bind query service on "
            << config.server.query_endpoint;
        return false;
    }

    log_info(LOG_SERVICE)
        << "Bound query service on "
        << config.server.query_endpoint;

    // This binds the heartbeat service.
    const auto rc = heartbeat_socket_.bind(
        config.server.heartbeat_endpoint.to_string());
    if (rc == 0)
    {
        log_error(LOG_SERVICE)
            << "Failed to bind heartbeat service on "
            << config.server.heartbeat_endpoint;
        return false;
    }

    log_info(LOG_SERVICE)
        << "Bound heartbeat service on "
        << config.server.heartbeat_endpoint;

    heartbeat_at_ = now() + heartbeat_interval_;
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

void request_worker::whitelist(const config::authority::list& addresses)
{
    for (const auto& ip_address: addresses)
    {
        log_info(LOG_SERVICE)
            << "Whitelisted client [" << format_whitelist(ip_address) << "]";

        authenticate_.allow(ip_address.to_string());
    }
}

bool request_worker::enable_crypto(const settings_type& config)
{
    const auto server_cert_path = config.server.certificate_file.string();
    auto client_certs_path = config.server.client_certificates_path.string();

    if (!client_certs_path.empty() && server_cert_path.empty())
    {
        log_error(LOG_SERVICE)
            << "Client authentication requires a server certificate.";

        return false;
    }

    // Configure server certificate if specified.
    if (!server_cert_path.empty())
    {
        certificate_.reset(server_cert_path);

        if (!certificate_.valid())
            return false;

        certificate_.apply(socket_);
        socket_.set_curve_server(zmq_curve_enabled);

        log_info(LOG_SERVICE)
            << "Loaded server certificate: " << server_cert_path;
    }

    // Always configure client certificates directory or *.
    if (client_certs_path.empty())
        client_certs_path = CURVE_ALLOW_ANY;
    else
        log_info(LOG_SERVICE)
        << "Require client certificates: " << client_certs_path;

    static const auto all_domains = "*";
    authenticate_.configure_curve(all_domains, client_certs_path);
    return true;
}

bool request_worker::create_new_socket(const settings_type& config)
{
    // Not sure what we would use this for, so disabled for now.
    // Set the socket identity name.
    //if (!config.unique_name.get_host().empty())
    //    socket_.set_identity(config.unique_name.get_host());

    // Connect...
    // Returns port number if connected.
    const auto rc = socket_.bind(config.server.query_endpoint.to_string());
    if (rc != 0)
    {
        socket_.set_linger(zmq_socket_no_linger);
        return true;
    }

    return false;
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
    czmqpp::socket which = poller.wait(polling_interval_milliseconds_);
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
            if (log_requests_)
                log_debug(LOG_REQUEST)
                    << "Service request [" << request.command() << "] from "
                    << encode_base16(request.origin());

            it->second(request,
                std::bind(&send_worker::queue_send,
                    &sender_, _1));
        }
        else
        {
            log_warning(LOG_SERVICE)
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
    if (now() > heartbeat_at_)
    {
        heartbeat_at_ = now() + heartbeat_interval_;
        log_debug(LOG_SERVICE) << "Publish service heartbeat";
        publish_heartbeat();
    }
}

void request_worker::publish_heartbeat()
{
    static uint32_t counter = 0;
    czmqpp::message message;
    const auto raw_counter = to_data_chunk(to_little_endian(counter));
    message.append(raw_counter);
    message.send(heartbeat_socket_);
    ++counter;
}

} // namespace server
} // namespace libbitcoin
