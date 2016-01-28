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
#include <bitcoin/server/messaging/incoming_message.hpp>
#include <bitcoin/server/messaging/outgoing_message.hpp>

#include <cstdint>
#include <random>
#include <string>

namespace libbitcoin {
namespace server {

// ----------------------------------------------------------------------------
// Constructors

outgoing_message::outgoing_message()
{
}

outgoing_message::outgoing_message(const data_chunk& destination,
    const std::string& command, const data_chunk& data)
  : id_(rand()),
    data_(data),
    command_(command),
    destination_(destination)
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

// ----------------------------------------------------------------------------
// Actions

void outgoing_message::send(czmqpp::socket& socket) const
{
    czmqpp::message message;

    // Optional, ROUTER sockets strip this.
    if (!destination_.empty())
        message.append(destination_);

    message.append({ command_.begin(), command_.end() });
    message.append(to_chunk(to_little_endian(id_)));
    message.append(data_);

    message.send(socket);
}

// ----------------------------------------------------------------------------
// Properties

uint32_t outgoing_message::id() const
{
    return id_;
}

const data_chunk& outgoing_message::data() const
{
    return data_;
}

const std::string& outgoing_message::command() const
{
    return command_;
}

const data_chunk outgoing_message::destination() const
{
    return destination_;
}

} // namespace server
} // namespace libbitcoin
