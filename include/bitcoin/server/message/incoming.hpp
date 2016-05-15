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
#ifndef LIBBITCOIN_SERVER_INCOMING
#define LIBBITCOIN_SERVER_INCOMING

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class BCS_API incoming
{
public:
    bool receive(bc::protocol::zmq::socket& socket);

    uint32_t id() const;
    const data_chunk& data() const;
    const std::string& command() const;
    const data_chunk origin() const;

private:
    uint32_t id_;
    data_chunk data_;
    std::string command_;
    data_chunk origin_;
};

} // namespace server
} // namespace libbitcoin

#endif
