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
#ifndef LIBBITCOIN_SERVER_BLOCK_SERVICE_HPP
#define LIBBITCOIN_SERVER_BLOCK_SERVICE_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

class server_node;

class BCS_API block_service
  : public enable_shared_from_base<block_service>
{
public:
    typedef std::shared_ptr<block_service> ptr;

    /// Construct a block endpoint.
    block_service(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// This class is not copyable.
    block_service(const block_service&) = delete;
    void operator=(const block_service&) = delete;

    /// Start the endpoint.
    bool start();

    /// Stop the endpoint.
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
