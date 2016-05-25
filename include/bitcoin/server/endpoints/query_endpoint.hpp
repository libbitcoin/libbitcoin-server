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
#ifndef LIBBITCOIN_SERVER_QUERY_ENDPOINT_HPP
#define LIBBITCOIN_SERVER_QUERY_ENDPOINT_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/settings.hpp>

/// This class must be constructed as a shared pointer.
namespace libbitcoin {
namespace server {

class server_node;

class BCS_API query_endpoint
  : public enable_shared_from_base<query_endpoint>
{
public:
    typedef std::shared_ptr<query_endpoint> ptr;
    typedef std::function<void(const incoming&, send_handler)> command_handler;

    query_endpoint(bc::protocol::zmq::context::ptr context, server_node* node);

    /// This class is not copyable.
    query_endpoint(const query_endpoint&) = delete;
    void operator=(const query_endpoint&) = delete;

    bool start();
    void stop();
    bool stopped() const;

    void attach(const std::string& command, command_handler handler);
    void poll(uint32_t interval_milliseconds);

private:
    typedef std::unordered_map<std::string, command_handler> command_map;

    void dequeue();
    void enqueue(outgoing& message);

    command_map handlers_;
    std::atomic<bool> stopped_;
    dispatcher dispatch_;
    const settings& settings_;

    // This polls the query socket *and* the internal queue.
    bc::protocol::zmq::poller poller_;
    bc::protocol::zmq::socket query_socket_;

    // The push/pull sockets form an internal queue.
    bc::protocol::zmq::context context_;
    bc::protocol::zmq::socket push_socket_;
    bc::protocol::zmq::socket pull_socket_;
};

} // namespace server
} // namespace libbitcoin

#endif
