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

#define LOG_PUBLISHER LOG_WORKER

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

bool publisher::setup_socket(
    const std::string& connection, czmqpp::socket& socket)
{
    if (connection.empty())
        return true;
    return socket.bind(connection) != -1;
}

bool publisher::start(settings_type& config)
{
    node_.subscribe_blocks(std::bind(&publisher::send_blk, this, _1, _2));
    node_.subscribe_transactions(std::bind(&publisher::send_tx, this, _1));
    log_debug(LOG_PUBLISHER) << "Publishing blocks: "
        << config.block_publish;
    if (!setup_socket(config.block_publish, socket_block_))
        return false;
    log_debug(LOG_PUBLISHER) << "Publishing transactions: "
        << config.tx_publish;
    if (!setup_socket(config.tx_publish, socket_tx_))
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

void publisher::send_blk(uint32_t height, const block_type& blk)
{
    // Serialize the height.
    data_chunk raw_height = to_data_chunk(to_little_endian(height));
    BITCOIN_ASSERT(raw_height.size() == sizeof(uint32_t));
    // Serialize the 80 byte header.
    data_chunk raw_blk_header(bc::satoshi_raw_size(blk.header));
    DEBUG_ONLY(auto it =) satoshi_save(blk.header, raw_blk_header.begin());
    BITCOIN_ASSERT(it == raw_blk_header.end());
    BITCOIN_ASSERT(raw_blk_header.size() == 80);
    // Construct the message.
    //   height   [4 bytes]
    //   header   [80 bytes]
    //   ... txs ...
    czmqpp::message message;
    message.append(raw_height);
    message.append(raw_blk_header);
    // Clients should be buffering their unconfirmed txs
    // and only be requesting those they don't have.
    for (const bc::transaction_type& tx: blk.transactions)
        append_hash(message, hash_transaction(tx));
    // Finished. Send message.
    if (!message.send(socket_block_))
    {
        log_warning(LOG_PUBLISHER) << "Problem publishing block data.";
        return;
    }
}

void publisher::send_tx(const transaction_type& tx)
{
    data_chunk raw_tx(bc::satoshi_raw_size(tx));
    DEBUG_ONLY(auto it =) satoshi_save(tx, raw_tx.begin());
    BITCOIN_ASSERT(it == raw_tx.end());
    czmqpp::message message;
    message.append(raw_tx);
    if (!message.send(socket_tx_))
    {
        log_warning(LOG_PUBLISHER) << "Problem publishing tx data.";
        return;
    }
}

} // namespace server
} // namespace libbitcoin
