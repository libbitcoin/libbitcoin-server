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
#ifndef LIBBITCOIN_SERVER_BLOCK_ENDPOINT_HPP
#define LIBBITCOIN_SERVER_BLOCK_ENDPOINT_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

class server_node;

/// This class must be constructed as a shared pointer.
class BCS_API block_endpoint
  : public enable_shared_from_base<block_endpoint>
{
public:
    typedef std::shared_ptr<block_endpoint> ptr;

    /// Construct a block endpoint.
    block_endpoint(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// This class is not copyable.
    block_endpoint(const block_endpoint&) = delete;
    void operator=(const block_endpoint&) = delete;

    /// Subscribe to block notifications and relay blocks.
    bool start();

    /// Stop the socket.
    bool stop();

private:
    void send(uint32_t height, const chain::block::ptr block);

    server_node& node_;
    bc::protocol::zmq::socket socket_;
    const bc::config::endpoint endpoint_;
    const bool enabled_;
    const bool secure_;
};

} // namespace server
} // namespace libbitcoin

#endif
