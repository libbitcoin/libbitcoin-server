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
#ifndef LIBBITCOIN_SERVER_WORKER_HPP
#define LIBBITCOIN_SERVER_WORKER_HPP

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <czmq++/czmqpp.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

/**
 * We don't want to block the originating threads that execute a send
 * as that would slow down requests if they all have to sync access
 * to a single socket.
 *
 * Instead we have a queue (push socket) where send requests are pushed,
 * and then the send_worker is notified. The worker wakes up and pushes
 * all pending requests to the socket.
 */
class send_worker
{
public:
    send_worker(czmqpp::context& context);
    void queue_send(const outgoing_message& message);

private:
    czmqpp::context& context_;
};

class request_worker
{
public:
    typedef std::function<void(
        const incoming_message&, queue_send_callback)> command_handler;

    request_worker();
    bool start(settings_type& config);
    void stop();
    void attach(const std::string& command, command_handler handler);
    void update();

private:
    typedef std::unordered_map<std::string, command_handler> command_map;

    void whitelist(std::vector<endpoint_type>& addrs);
    void enable_crypto(settings_type& config);
    void create_new_socket(settings_type& config);
    void poll();
    void publish_heartbeat();

    czmqpp::context context_;
    // Main socket.
    czmqpp::socket socket_;
    czmqpp::authenticator auth_;
    // Socket to trigger wakeup for send.
    czmqpp::socket wakeup_socket_;
    // We publish a heartbeat every so often so clients
    // can know our availability.
    czmqpp::socket heartbeat_socket_;

    // Send out heartbeats at regular intervals
    boost::posix_time::ptime heartbeat_at_;

    command_map handlers_;
    send_worker sender_;

    bool log_requests_ = false;
};

} // namespace server
} // namespace libbitcoin

#endif

