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
#ifndef LIBBITCOIN_SERVER_MESSAGE
#define LIBBITCOIN_SERVER_MESSAGE

#include <czmq++/czmqpp.hpp>
#include <bitcoin/bitcoin.hpp>

namespace server {

class incoming_message
{
public:
    bool recv(czmqpp::socket& socket);
    const bc::data_chunk origin() const;
    const std::string& command() const;
    uint32_t id() const;
    const bc::data_chunk& data() const;

private:
    bc::data_chunk origin_;
    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

class outgoing_message
{
public:
    // Empty dest = unspecified destination.
    outgoing_message(
        const bc::data_chunk& dest, const std::string& command,
        const bc::data_chunk& data);
    outgoing_message(
        const incoming_message& request, const bc::data_chunk& data);
    // Default constructor provided for containers and copying.
    outgoing_message();

    void send(czmqpp::socket& socket) const;
    uint32_t id() const;

private:
    bc::data_chunk dest_;
    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

} // namespace server

#endif

