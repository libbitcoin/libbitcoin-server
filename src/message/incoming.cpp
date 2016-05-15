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
#include <bitcoin/server/message/incoming.hpp>

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

// Actions
// ----------------------------------------------------------------------------

bool incoming::receive(zmq::socket& socket)
{
    zmq::message message;
    message.receive(socket);
    const auto& parts = message.parts();

    if (parts.size() < 3 || parts.size() > 4)
        return false;

    auto it = parts.begin();

    // Optional - ROUTER sockets strip this.
    if (parts.size() == 4)
    {
        origin_ = *it;
        ++it;
    }

    const auto& command = *it;
    command_ = std::string(command.begin(), command.end());
    ++it;

    const auto& id = *it;
    if (id.size() != sizeof(uint32_t))
        return false;

    id_ = from_little_endian_unsafe<uint32_t>(id.begin());
    ++it;

    data_ = *it;
    ++it;

    BITCOIN_ASSERT(it == parts.end());
    return true;
}

// Properties
// ----------------------------------------------------------------------------

uint32_t incoming::id() const
{
    return id_;
}

const data_chunk& incoming::data() const
{
    return data_;
}

const std::string& incoming::command() const
{
    return command_;
}

const data_chunk incoming::origin() const
{
    return origin_;
}

} // namespace server
} // namespace libbitcoin
