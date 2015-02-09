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
#include <random>
#include "message.hpp"

namespace libbitcoin {
namespace server {

bool incoming_message::recv(czmqpp::socket& socket)
{
    czmqpp::message message;
    message.receive(socket);
    const data_stack& parts = message.parts();
    if (parts.size() != 3 && parts.size() != 4)
        return false;
    auto it = parts.begin();
    // [ DESTINATION ] (optional - ROUTER sockets strip this)
    if (parts.size() == 4)
    {
        origin_ = *it;
        ++it;
    }
    // [ COMMAND ]
    const data_chunk& raw_command = *it;
    command_ = std::string(raw_command.begin(), raw_command.end());
    ++it;
    // [ ID ]
    const data_chunk& raw_id = *it;
    if (raw_id.size() != sizeof(uint32_t))
        return false;
    id_ = from_little_endian_unsafe<uint32_t>(raw_id.begin());
    ++it;
    // [ DATA ]
    data_ = *it;
    ++it;
    BITCOIN_ASSERT(it == parts.end());
    return true;
}

const bc::data_chunk incoming_message::origin() const
{
    return origin_;
}
const std::string& incoming_message::command() const
{
    return command_;
}
uint32_t incoming_message::id() const
{
    return id_;
}
const data_chunk& incoming_message::data() const
{
    return data_;
}

outgoing_message::outgoing_message()
{
}

outgoing_message::outgoing_message(
    const data_chunk& dest, const std::string& command,
    const data_chunk& data)
  : dest_(dest), command_(command),
    id_(rand()), data_(data)
{
}

outgoing_message::outgoing_message(
    const incoming_message& request, const data_chunk& data)
  : dest_(request.origin()), command_(request.command()),
    id_(request.id()), data_(data)
{
}

void append_str(czmqpp::message& message, const std::string& command)
{
    message.append(data_chunk(command.begin(), command.end()));
}

void outgoing_message::send(czmqpp::socket& socket) const
{
    czmqpp::message message;
    // [ DESTINATION ] (optional - ROUTER sockets strip this)
    if (!dest_.empty())
        message.append(dest_);
    // [ COMMAND ]
    append_str(message, command_);
    // [ ID ]
    data_chunk raw_id = to_data_chunk(to_little_endian(id_));
    BITCOIN_ASSERT(raw_id.size() == sizeof(id_));
    message.append(raw_id);
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
