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
#ifndef LIBBITCOIN_SERVER_QUERY_SOCKET_HPP
#define LIBBITCOIN_SERVER_QUERY_SOCKET_HPP

#include <memory>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/web/manager.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Submit queries and address subscriptions and receive address notifications.
class BCS_API query_socket
  : public manager
{
public:
    typedef std::shared_ptr<query_socket> ptr;

    /// Construct a query service.
    query_socket(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    // Implement the socket.
    virtual void work();

    virtual bool start_websocket_handler() override;

    // Initialize the query specific zmq socket.
    virtual void handle_websockets() override;

    virtual const config::endpoint& retrieve_zeromq_endpoint() const override;
    virtual const config::endpoint& retrieve_websocket_endpoint() const override;

    const config::endpoint& retrieve_query_endpoint() const;
    const config::endpoint& retrieve_zeromq_client_endpoint() const;
};

} // namespace server
} // namespace libbitcoin

#endif
