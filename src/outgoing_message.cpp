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
#include <bitcoin/server/incoming_message.hpp>
#include <bitcoin/server/outgoing_message.hpp>

#include <cstdint>
#include <random>
#include <string>

namespace libbitcoin {
namespace server {

outgoing_message::outgoing_message()
{
}

outgoing_message::outgoing_message(const data_chunk& destination,
    const std::string& command, const data_chunk& data)
  : destination_(destination),
    command_(command),
    id_(rand()),
    data_(data)
{
}

outgoing_message::outgoing_message(const incoming_message& request,
    const data_chunk& data)
  : destination_(request.origin()),
    command_(request.command()),
    id_(request.id()),
    data_(data)
{
}

void outgoing_message::send(czmqpp::socket& socket) const
{
    czmqpp::message message;

    // [ DESTINATION ] (optional - ROUTER sockets strip this)
    if (!destination_.empty())
        message.append(destination_);

    // [ COMMAND ]
    message.append({ command_.begin(), command_.end() });

    // [ ID ]
    const auto id = to_chunk(to_little_endian(id_));
    BITCOIN_ASSERT(id.size() == sizeof(id_));
    message.append(id);

    // [ DATA ]
    message.append(data_);

    // Send.
    message.send(socket);
}

uint32_t outgoing_message::id() const
{
    return id_;
}

} // namespace server
} // namespace libbitcoin
