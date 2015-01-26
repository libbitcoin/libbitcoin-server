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
#include <cstdint>
#include <vector>
#include <bitcoin/bitcoin.hpp>
#include "echo.hpp"
#include "worker.hpp"

namespace libbitcoin {
namespace server {
    
namespace posix_time = boost::posix_time;
using posix_time::milliseconds;
using posix_time::seconds;
using posix_time::microsec_clock;
using std::placeholders::_1;

const posix_time::time_duration heartbeat_interval = milliseconds(4000);

// Milliseconds
constexpr uint32_t poll_sleep_interval = 1000;

auto now = []() { return microsec_clock::universal_time(); };

send_worker::send_worker(czmqpp::context& context)
  : context_(context)
{
}
void send_worker::queue_send(const outgoing_message& message)
{
    czmqpp::socket socket(context_, ZMQ_PUSH);
    BITCOIN_ASSERT(socket.self());
    DEBUG_ONLY(int rc =) socket.connect("inproc://trigger-send");
    BITCOIN_ASSERT(rc == 0);
    message.send(socket);
    socket.destroy(context_);
}

request_worker::request_worker()
  : socket_(context_, ZMQ_ROUTER),
    auth_(context_),
    wakeup_socket_(context_, ZMQ_PULL),
    heartbeat_socket_(context_, ZMQ_PUB),
    sender_(context_)
{
    BITCOIN_ASSERT(socket_.self());
    BITCOIN_ASSERT(wakeup_socket_.self());
    BITCOIN_ASSERT(heartbeat_socket_.self());
    DEBUG_ONLY(int rc =) wakeup_socket_.bind("inproc://trigger-send");
    BITCOIN_ASSERT(rc != -1);
}
bool request_worker::start(settings_type& config)
{
    // Load config values.
    log_requests_ = config.log_requests;
    if (log_requests_)
        auth_.set_verbose(true);
    if (!config.clients.empty())
        whitelist(config.clients);
    if (config.certificate.empty())
        socket_.set_zap_domain("global");
    else
        enable_crypto(config);
    // Start ZeroMQ sockets.
    create_new_socket(config);
    log_debug(LOG_WORKER) << "Heartbeat: " << config.heartbeat;
    heartbeat_socket_.bind(config.heartbeat);
    // Timer stuff
    heartbeat_at_ = now() + heartbeat_interval;
    return true;
}
void request_worker::stop()
{
}

void request_worker::whitelist(std::vector<endpoint_type>& addrs)
{
    for (const auto& ip_address: addrs)
        auth_.allow(ip_address);
}
void request_worker::enable_crypto(settings_type& config)
{
    if (config.client_certs_path.empty())
        auth_.configure_curve("*", CURVE_ALLOW_ANY);
    else
        auth_.configure_curve("*", config.client_certs_path.generic_string());
    cert_.reset(czmqpp::load_cert(config.certificate));
    cert_.apply(socket_);
    socket_.set_curve_server(1);
}
void request_worker::create_new_socket(settings_type& config)
{
    log_debug(LOG_WORKER) << "Listening: " << config.service;
    // Set the socket identity name.
    if (!config.unique_name.get_host().empty())
        socket_.set_identity(config.unique_name.get_host());
    // Connect...
    socket_.bind(config.service);
    // Configure socket to not wait at close time
    socket_.set_linger(0);
    // Tell queue we're ready for work
    log_info(LOG_WORKER) << "worker ready";
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
    BITCOIN_ASSERT(poller.self());
    czmqpp::socket which = poller.wait(poll_sleep_interval);

    BITCOIN_ASSERT(socket_.self() && wakeup_socket_.self());
    if (which == socket_)
    {
        // Get message
        // 6-part envelope + content -> request
        incoming_message request;
        request.recv(socket_);

        auto it = handlers_.find(request.command());
        // Perform request if found.
        if (it != handlers_.end())
        {
            if (log_requests_)
                log_debug(LOG_REQUEST) << request.command()
                    << " from " << encode_base16(request.origin());
            it->second(request,
                std::bind(&send_worker::queue_send, &sender_, _1));
        }
        else
        {
            log_warning(LOG_WORKER)
                << "Unhandled request: " << request.command()
                << " from " << encode_base16(request.origin());
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
        heartbeat_at_ = now() + heartbeat_interval;
        log_debug(LOG_WORKER) << "Sending heartbeat";
        publish_heartbeat();
    }
}

void request_worker::publish_heartbeat()
{
    static uint32_t counter = 0;
    czmqpp::message message;
    data_chunk raw_counter = to_data_chunk(to_little_endian(counter));
    message.append(raw_counter);
    message.send(heartbeat_socket_);
    ++counter;
}

} // namespace server
} // namespace libbitcoin
