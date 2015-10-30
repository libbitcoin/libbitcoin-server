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
#include <bitcoin/server/publisher.hpp>

#include <bitcoin/server/config/config.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;

publisher::publisher(server_node& node)
  : node_(node),
    socket_block_(context_, ZMQ_PUB),
    socket_tx_(context_, ZMQ_PUB)
{
}

bool publisher::setup_socket(const std::string& connection,
    czmqpp::socket& socket)
{
    if (connection.empty())
        return true;

    return socket.bind(connection) != -1;
}

bool publisher::start(const settings_type& config)
{
    node_.subscribe_blocks(std::bind(&publisher::send_block, this, _1, _2));
    node_.subscribe_transactions(std::bind(&publisher::send_tx, this, _1));

    log::debug(LOG_PUBLISHER) << "Publishing blocks on "
        << config.server.block_publish_endpoint;
    if (!setup_socket(config.server.block_publish_endpoint.to_string(),
        socket_block_))
        return false;

    log::debug(LOG_PUBLISHER) << "Publishing transactions on "
        << config.server.transaction_publish_endpoint;
    if (!setup_socket(config.server.transaction_publish_endpoint.to_string(),
        socket_tx_))
        return false;

    return true;
}

bool publisher::stop()
{
    return true;
}

void append_hash(czmqpp::message& message, const hash_digest& hash)
{
    message.append(data_chunk(hash.begin(), hash.end()));
}

void publisher::send_block(uint32_t height, const chain::block& block)
{
    // Serialize the height.
    data_chunk raw_height = to_chunk(to_little_endian(height));
    BITCOIN_ASSERT(raw_height.size() == sizeof(uint32_t));

    // Serialize the 80 byte header.
    data_chunk raw_block_header = block.header.to_data(false);
    BITCOIN_ASSERT(raw_block_header.size() == 80);

    // Construct the message.
    //   height   [4 bytes]
    //   header   [80 bytes]
    //   ... txs ...
    czmqpp::message message;
    message.append(raw_height);
    message.append(raw_block_header);

    // Clients should be buffering their unconfirmed txs
    // and only be requesting those they don't have.
    for (const auto& tx: block.transactions)
        append_hash(message, tx.hash());

    // Finished. Send message.
    if (!message.send(socket_block_))
    {
        log::warning(LOG_PUBLISHER) << "Problem publishing block data.";
        return;
    }
}

void publisher::send_tx(const chain::transaction& tx)
{
    data_chunk raw_tx = tx.to_data();
    czmqpp::message message;
    message.append(raw_tx);

    if (!message.send(socket_tx_))
    {
        log::warning(LOG_PUBLISHER) << "Problem publishing tx data.";
        return;
    }
}

} // namespace server
} // namespace libbitcoin
