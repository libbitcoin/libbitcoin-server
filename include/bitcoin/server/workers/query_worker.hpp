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
#ifndef LIBBITCOIN_SERVER_QUERY_WORKER_HPP
#define LIBBITCOIN_SERVER_QUERY_WORKER_HPP

#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Provide asynchronous query responses to the query service.
class BCS_API query_worker
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<query_worker> ptr;

    /// Construct a query worker.
    query_worker(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    typedef bc::protocol::zmq::socket socket;

    typedef std::function<void(const message&, send_handler)> command_handler;
    typedef std::unordered_map<std::string, command_handler> command_map;

    virtual void attach_interface();
    virtual void attach(const std::string& command, command_handler handler);

    virtual bool connect(socket& dealer);
    virtual bool disconnect(socket& dealer);
    virtual void query(socket& dealer);

    // Implement the worker.
    virtual void work();

private:
    static void send(const message& response,
        bc::protocol::zmq::socket& dealer);

    // These are thread safe.
    const bool secure_;
    const std::string security_;
    const bc::server::settings& settings_;
    const bc::protocol::settings& external_;
    const bc::protocol::settings internal_;
    const config::endpoint& worker_;
    bc::protocol::zmq::authenticator& authenticator_;
    server_node& node_;

    // This is protected by worker base class mutex.
    command_map command_handlers_;
};

} // namespace server
} // namespace libbitcoin

#endif
