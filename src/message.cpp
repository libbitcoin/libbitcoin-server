#include <obelisk/message.hpp>

#include <random>
#include <bitcoin/format.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/sha256.hpp>
#include <obelisk/zmq_message.hpp>

namespace obelisk {

using namespace bc;

bool incoming_message::recv(zmq::socket_t& socket)
{
    zmq_message message;
    message.recv(socket);
    const data_stack& parts = message.parts();
    if (parts.size() != 4 && parts.size() != 5)
        return false;
    auto it = parts.begin();
    // [ DESTINATION ] (optional - ROUTER sockets strip this)
    if (parts.size() == 5)
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
    if (raw_id.size() != 4)
        return false;
    id_ = cast_chunk<uint32_t>(raw_id);
    ++it;
    // [ DATA ]
    data_ = *it;
    ++it;
    // [ CHECKSUM ]
    const data_chunk& raw_checksum = *it;
    uint32_t checksum = cast_chunk<uint32_t>(raw_checksum);
    if (checksum != generate_sha256_checksum(data_))
        return false;
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
const uint32_t incoming_message::id() const
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

void append_str(zmq_message& message, const std::string& command)
{
    message.append(data_chunk(command.begin(), command.end()));
}

void outgoing_message::send(zmq::socket_t& socket) const
{
    zmq_message message;
    // [ DESTINATION ] (optional - ROUTER sockets strip this)
    if (!dest_.empty())
        message.append(dest_);
    // [ COMMAND ]
    append_str(message, command_);
    // [ ID ]
    data_chunk raw_id = uncast_type(id_);
    BITCOIN_ASSERT(raw_id.size() == 4);
    message.append(raw_id);
    // [ DATA ]
    message.append(data_);
    // [ CHECKSUM ]
    data_chunk raw_checksum = uncast_type(generate_sha256_checksum(data_));
    BITCOIN_ASSERT(raw_checksum.size() == 4);
    message.append(raw_checksum);
    // Send.
    message.send(socket);
}

const uint32_t outgoing_message::id() const
{
    return id_;
}

} // namespace obelisk

