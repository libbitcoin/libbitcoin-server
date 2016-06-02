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
    query_worker(bc::protocol::zmq::context& context, server_node& node);

    /// Start the worker (restartable).
    bool start() override;

    /// Stop the worker (idempotent).
    bool stop() override;

private:
    typedef std::function<void(const incoming&, send_handler)> command_handler;
    typedef std::unordered_map<std::string, command_handler> command_map;

    void attach_interface();
    void attach(const std::string& command, command_handler handler);
    ////void receive(bc::protocol::zmq::socket& socket);
    ////void send(outgoing& response, const config::endpoint& query_worker);

    // Implement the worker.
    virtual void work();

    const bool log_;
    const bool enabled_;

    // These are protected by mutex.
    command_map handlers_;
    address_notifier address_notifier_;
    mutable shared_mutex mutex_;

    // This is thread safe.
    bc::protocol::zmq::context& context_;
};

} // namespace server
} // namespace libbitcoin

#endif
