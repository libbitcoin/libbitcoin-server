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
#ifndef LIBBITCOIN_SERVER_WEB_BLOCK_SOCKET_HPP
#define LIBBITCOIN_SERVER_WEB_BLOCK_SOCKET_HPP

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/web/socket.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Subscribe to block acceptances into the long chain from a dedicated
// socket endpoint.
class BCS_API block_socket
  : public socket
{
public:
    typedef std::shared_ptr<block_socket> ptr;

    /// Construct a block socket service endpoint.
    block_socket(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    // Implement the service.
    virtual void work() override;

    virtual const config::endpoint& retrieve_zeromq_endpoint() const override;
    virtual const config::endpoint& retrieve_websocket_endpoint()
        const override;

private:
    bool handle_block(bc::protocol::zmq::socket& subscriber);
};

} // namespace server
} // namespace libbitcoin

#endif
