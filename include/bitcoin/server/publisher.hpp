/*
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

#include <czmq++/czmqpp.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

class BCS_API publisher
{
public:
    publisher(server_node& node);
    bool start(settings_type& config);
    bool stop();

private:
    bool setup_socket(
        const std::string& connection, czmqpp::socket& socket);

    void send_blk(uint32_t height, const bc::block_type& blk);
    void send_tx(const bc::transaction_type& tx);

    server_node& node_;
    czmqpp::context context_;
    czmqpp::socket socket_block_, socket_tx_;
};

} // namespace server
} // namespace libbitcoin

#endif
