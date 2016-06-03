/**
 * Copyright (c) 2016 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_QUERY_WORKER_HPP
#define LIBBITCOIN_SERVER_QUERY_WORKER_HPP

#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/address_notifier.hpp>

namespace libbitcoin {
namespace server {

class server_node;

class BCS_API query_worker
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<query_worker> ptr;

    /// Construct a query worker.
    query_worker(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    typedef std::function<void(const incoming&, send_handler)> command_handler;
    typedef std::unordered_map<std::string, command_handler> command_map;

    virtual void attach(const std::string& command, command_handler handler);
    virtual void attach_interface();

    virtual bool connect(bc::protocol::zmq::socket& socket);
    virtual bool disconnect(bc::protocol::zmq::socket& socket);

    virtual void query(bc::protocol::zmq::socket& socket);
    virtual void handle_query(outgoing& response,
        bc::protocol::zmq::socket& socket);

    // Implement the worker.
    virtual void work();

private:
    const bool secure_;
    const server::settings& settings_;

    // These are thread safe.
    server_node& node_;
    address_notifier address_notifier_;
    bc::protocol::zmq::authenticator& authenticator_;

    // This is protected by base class mutex.
    command_map command_handlers_;
};

} // namespace server
} // namespace libbitcoin

#endif
