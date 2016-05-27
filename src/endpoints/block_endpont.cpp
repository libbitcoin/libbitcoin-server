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
#include <bitcoin/server/endpoints/block_endpoint.hpp>

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

#define NAME "block_endpoint"

using std::placeholders::_1;
using std::placeholders::_2;
using namespace bc::chain;
using namespace bc::protocol;

block_endpoint::block_endpoint(zmq::authenticator& authenticator,
    server_node* node)
  : node_(node),
    socket_(authenticator, zmq::socket::role::pusher),
    settings_(node->server_settings())
{
    if (!settings_.block_endpoint_enabled)
        return;

    const auto secure = settings_.server_private_key;

    if (!authenticator.apply(socket_, NAME, secure))
        socket_.stop();
}

// The instance is retained in scope by subscribe_blocks until stopped.
bool block_endpoint::start()
{
    if (!settings_.block_endpoint_enabled)
    {
        stop();
        return true;
    }
    
    auto block_endpoint = settings_.block_endpoint;

    if (!socket_ || !socket_.bind(block_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize block publisher on " << block_endpoint;
        stop();
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound block publish service on " << block_endpoint;

    // This is not a libbitcoin re/subscriber.
    node_->subscribe_blocks(
        std::bind(&block_endpoint::send,
            this, _1, _2));

    return true;
}

bool block_endpoint::stop()
{
    return socket_.stop();
}

void block_endpoint::send(uint32_t height, const block::ptr block)
{
    zmq::message message;
    message.enqueue_little_endian(height);
    message.enqueue(block->header.to_data(false));

    for (const auto& tx: block->transactions)
        message.enqueue(tx.hash());

    if (!message.send(socket_))
        log::warning(LOG_ENDPOINT)
            << "Failure publishing block data.";
}

} // namespace server
} // namespace libbitcoin
