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
#ifndef LIBBITCOIN_SERVER_WEB_QUERY_SOCKET_HPP
#define LIBBITCOIN_SERVER_WEB_QUERY_SOCKET_HPP

#include <memory>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/web/http/socket.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Submit queries and address subscriptions and receive address
// notifications on a dedicated socket endpoint.
class BCS_API query_socket
  : public http::socket
{
public:
    typedef std::shared_ptr<query_socket> ptr;

    /// Construct a query socket service endpoint.
    query_socket(bc::protocol::zmq::context& context, server_node& node,
        bool secure);

protected:
    // Implement the socket.
    virtual void work() override;

    virtual bool start_websocket_handler() override;

    // Initialize the query specific zmq socket.
    virtual void handle_websockets() override;

    virtual const config::endpoint& zeromq_endpoint() const override;
    virtual const config::endpoint& websocket_endpoint() const override;
    virtual const std::shared_ptr<bc::protocol::zmq::socket> service()
        const override;

    const config::endpoint& query_endpoint() const;

private:
    bool handle_query(bc::protocol::zmq::socket& dealer);

    std::shared_ptr<bc::protocol::zmq::socket> service_;
};

} // namespace server
} // namespace libbitcoin

#endif
