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
#ifndef LIBBITCOIN_SERVER_PUBLISHER_HPP
#define LIBBITCOIN_SERVER_PUBLISHER_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// The publisher subscribes to blocks accepted to the blockchain and
/// transactions accepted to the memory pool. The blocks and transactions
/// are then forwarded to its notifiers.
class BCS_API publisher
  : public enable_shared_from_base<publisher>
{
public:
    typedef std::shared_ptr<publisher> ptr;

    publisher(server_node::ptr node);

    /// This class is not copyable.
    publisher(const publisher&) = delete;
    void operator=(const publisher&) = delete;

    bool start();

private:
    void send_tx(const chain::transaction& tx);
    void send_block(uint32_t height, const chain::block::ptr block);

    server_node::ptr node_;
    bc::protocol::zmq::context context_;
    bc::protocol::zmq::socket socket_tx_;
    bc::protocol::zmq::socket socket_block_;
    const settings& settings_;
};

} // namespace server
} // namespace libbitcoin

#endif
