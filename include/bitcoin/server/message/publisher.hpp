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
#include <czmq++/czmqpp.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// The publisher subscribes to blocks accepted to the blockchain and
/// transactions accepted to the memory pool. The blocks and transactions
/// are then forwarded to its subscribers.
class BCS_API publisher
{
public:
    publisher(server_node& node, const settings& settings);

    bool start();

private:
    void send_tx(const chain::transaction& tx);
    void send_block(uint32_t height, const chain::block& block);

    server_node& node_;
    czmqpp::context context_;
    czmqpp::socket socket_tx_;
    czmqpp::socket socket_block_;
    const settings& settings_;
};

} // namespace server
} // namespace libbitcoin

#endif
