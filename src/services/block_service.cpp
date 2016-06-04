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
#include <bitcoin/server/services/block_service.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define PUBLIC_NAME "public_block"
#define SECURE_NAME "secure_block"

using std::placeholders::_1;
using std::placeholders::_2;
using namespace bc::chain;
using namespace bc::protocol;

static inline bool is_enabled(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return settings.block_service_enabled &&
        (!secure || settings.server_private_key);
}

static inline config::endpoint get_endpoint(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return secure ? settings.secure_block_endpoint :
        settings.public_block_endpoint;
}

// ZMQ_PUSH (we might want ZMQ_SUB here)
// When a ZMQ_PUSH socket enters an exceptional state due to having reached the
// high water mark for all downstream nodes, or if there are no downstream
// nodes at all, then any zmq_send(3) operations on the socket shall block
// until the exceptional state ends or at least one downstream node becomes
//available for sending; messages are not discarded.
block_service::block_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : node_(node),
    socket_(authenticator, zmq::socket::role::pusher),
    endpoint_(get_endpoint(node, secure)),
    enabled_(is_enabled(node, secure)),
    secure_(secure)
{
    const auto name = secure ? SECURE_NAME : PUBLIC_NAME;

    // The authenticator logs apply failures and stopped socket halts start.
    if (!enabled_ || !authenticator.apply(socket_, name, secure))
        socket_.stop();
}

// The endpoint is not restartable.
// The instance is retained in scope by subscribe_blocks until stopped.
bool block_service::start()
{
    if (!enabled_)
        return true;

    if (!socket_)
    {
        log::error(LOG_SERVER)
            << "Failed to initialize block publish service.";
        return false;
    }

    const auto ec = socket_.bind(endpoint_);

    if (ec)
    {
        log::error(LOG_SERVER)
            << "Failed to bind block publish service to " << endpoint_
            << " : " << ec.message();
        stop();
        return false;
    }

    log::info(LOG_SERVER)
        << "Bound " << (secure_ ? "secure " : "public ")
        << "block publish service to " << endpoint_;

    // This is not a libbitcoin re/subscriber.
    node_.subscribe_blocks(
        std::bind(&block_service::send,
            this, _1, _2));

    return true;
}

bool block_service::stop()
{
    if (!socket_ || socket_.stop())
        return true;
    
    log::debug(LOG_SERVER)
        << "Failed to unbind block publish service from " << endpoint_;

    return false;
}

// BUGBUG: this must be translated to the socket thread.
void block_service::send(uint32_t height, const block::ptr block)
{
    zmq::message message;
    message.enqueue_little_endian(height);
    message.enqueue(block->header.to_data(false));

    for (const auto& tx: block->transactions)
        message.enqueue(tx.hash());

    auto ec = message.send(socket_);

    if (ec)
        log::warning(LOG_SERVER)
            << "Failed to publish block on " << endpoint_
            << " : " << ec.message();
}

} // namespace server
} // namespace libbitcoin
