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
#include <bitcoin/server/message/publisher.hpp>

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using namespace bc::chain;
using namespace bc::protocol;

static constexpr size_t header_size = 80;

// BUGBUG: The publisher uses a context independent of the receiver/notifier
// and is therefore independent of that security context (no auth/cert).
publisher::publisher(server_node::ptr node)
  : node_(node),
    settings_(node->server_settings()),
    socket_tx_(context_, zmq::socket::role::pusher),
    socket_block_(context_, zmq::socket::role::pusher)
{
    BITCOIN_ASSERT((bool)socket_tx_);
    BITCOIN_ASSERT((bool)socket_block_);
}

bool publisher::start()
{
    if (!settings_.publisher_enabled)
        return true;
    
    auto block_endpoint = settings_.block_publish_endpoint.to_string();
    if (!block_endpoint.empty())
    {
        if (!socket_block_.bind(block_endpoint))
        {
            log::error(LOG_SERVICE)
                << "Failed to start block publisher on "
                << settings_.block_publish_endpoint;
            return false;
        }

        log::info(LOG_SERVICE)
            << "Bound block publish service on "
            << settings_.block_publish_endpoint;
    }

    auto tx_endpoint = settings_.transaction_publish_endpoint.to_string();
    if (!tx_endpoint.empty())
    {
        if (!socket_tx_.bind(tx_endpoint))
        {
            log::error(LOG_SERVICE)
                << "Failed to start transaction publisher on "
                << settings_.block_publish_endpoint;
            return false;
        }

        log::info(LOG_SERVICE)
            << "Bound transaction publish service on "
            << settings_.transaction_publish_endpoint;
    }

    // These are not libbitcoin re/subscribers.
    node_->subscribe_transactions(
        std::bind(&publisher::send_tx,
            shared_from_this(), _1));

    node_->subscribe_blocks(
        std::bind(&publisher::send_block,
            shared_from_this(), _1, _2));

    return true;
}

void publisher::send_tx(const transaction& tx)
{
    zmq::message message;
    message.append(tx.to_data());

    if (!message.send(socket_tx_))
        log::warning(LOG_SERVICE)
            << "Problem publishing tx data.";
}

void publisher::send_block(uint32_t height, const block::ptr block)
{
    // Serialize the block height.
    const auto raw_height = to_chunk(to_little_endian(height));
    BITCOIN_ASSERT(raw_height.size() == sizeof(uint32_t));

    // Serialize the block header.
    const auto raw_block_header = block->header.to_data(false);
    BITCOIN_ASSERT(raw_block_header.size() == header_size);

    // Construct the message.
    //   height   [4 bytes]
    //   header   [80 bytes]
    //   ... txs ...
    zmq::message message;
    message.append(raw_height);
    message.append(raw_block_header);

    for (const auto& tx: block->transactions)
        message.append(to_chunk(tx.hash()));

    if (!message.send(socket_block_))
        log::warning(LOG_SERVICE)
            << "Problem publishing block data.";
}

} // namespace server
} // namespace libbitcoin
