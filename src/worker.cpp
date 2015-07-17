/*
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

namespace libbitcoin {
namespace server {
    
namespace posix_time = boost::posix_time;
using posix_time::milliseconds;
using posix_time::seconds;
using posix_time::microsec_clock;
using std::placeholders::_1;

constexpr int zmq_true = 1;
constexpr int zmq_false = 0;
constexpr int zmq_curve_enabled = zmq_true;
constexpr int zmq_socket_no_linger = zmq_false;
DEBUG_ONLY(constexpr int zmq_success = 0;)
DEBUG_ONLY(constexpr int zmq_fail = -1;)

auto now = []() { return microsec_clock::universal_time(); };

send_worker::send_worker(czmqpp::context& context)
  : context_(context)
{
}

void send_worker::queue_send(const outgoing_message& message)
{
    czmqpp::socket socket(context_, ZMQ_PUSH);
    BITCOIN_ASSERT(socket.self() != nullptr);
    DEBUG_ONLY(int rc =) socket.connect("inproc://trigger-send");
    BITCOIN_ASSERT(rc == zmq_success);
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
    DEBUG_ONLY(int rc =) wakeup_socket_.bind("inproc://trigger-send");
    BITCOIN_ASSERT(rc != zmq_fail);
}

bool request_worker::start(settings_type& config)
{
    // Use config values.
    log_requests_ = config.log_requests;

#ifdef _MSC_VER
    if (log_requests_)
        log_debug(LOG_SERVICE)
            << "Autnetication logging disabled on Windows";
#else
    // This exposes the log stream to non-utf8 text on Windows.
    // TODO: fix zeromq/czmq/czmqpp to be utf8 everywhere.
    if (log_requests_)
        authenticate_.set_verbose(true);
#endif 

    if (!config.clients.empty())
        whitelist(config.clients);

    if (config.cert_file.empty())
        socket_.set_zap_domain("global");
    else
        enable_crypto(config);

    // Start ZeroMQ sockets.
    create_new_socket(config);
    log_debug(LOG_SERVICE)
        << "Query service heartbeat on " << config.heartbeat_endpoint;

    heartbeat_socket_.bind(config.heartbeat_endpoint);

    // Timer stuff
    heartbeat_at_ = now() + heartbeat_interval_;
    return true;
}

void request_worker::stop()
{
}

void request_worker::whitelist(std::vector<node::endpoint_type>& addrs)
{
    for (const auto& ip_address: addrs)
        authenticate_.allow(ip_address);
}

void request_worker::enable_crypto(settings_type& config)
{
    std::string client_certs(CURVE_ALLOW_ANY);
    if (!config.client_certs_path.empty())
        client_certs = config.client_certs_path.string();

    authenticate_.configure_curve("*", client_certs);
    czmqpp::certificate cert(config.cert_file.string());
    cert.apply(socket_);
    socket_.set_curve_server(zmq_curve_enabled);
}

void request_worker::create_new_socket(settings_type& config)
{
    // Not sure what we would use this for, so disabled for now.
    // Set the socket identity name.
    //if (!config.unique_name.get_host().empty())
    //    socket_.set_identity(config.unique_name.get_host());

    // Connect...
    socket_.bind(config.query_endpoint);
    socket_.set_linger(zmq_socket_no_linger);

    log_info(LOG_SERVICE)
        << "Query service listening on " << config.query_endpoint;
}

void request_worker::attach(
    const std::string& command, command_handler handler)
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
