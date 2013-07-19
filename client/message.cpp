#include "message.hpp"

#include <bitcoin/format.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/sha256.hpp>
#include <random>

using namespace bc;

void zmsg2::append(const data_chunk& part)
{
    parts_.push_back(part);
}

void zmsg2::send(zmq::socket_t& socket)
{
    bool send_more = true;
    for (auto it = parts_.begin(); it != parts_.end(); ++it)
    {
        BITCOIN_ASSERT(send_more);
        const data_chunk& data = *it;
        std::cout << "Sending: " << data << std::endl;
        zmq::message_t message(data.size());
        uint8_t* message_data = reinterpret_cast<uint8_t*>(message.data());
        std::copy(data.begin(), data.end(), message_data);
        if (it == parts_.end() - 1)
            send_more = false;
        try
        {
            socket.send(message, send_more ? ZMQ_SNDMORE : 0);
        }
        catch (zmq::error_t error)
        {
            BITCOIN_ASSERT(error.num() != 0);
        }
    }
    parts_.clear();
}

outgoing_message::outgoing_message()
{
}

outgoing_message::outgoing_message(
    const std::string& command, const data_chunk& data)
  : command_(command), id_(rand()), data_(data)
{
}

outgoing_message::outgoing_message(
    const incoming_message& request, const data_chunk& data)
  : command_(request.command()), id_(request.id()), data_(data)
{
}

void outgoing_message::send(zmq::socket_t& socket)
{
    zmsg2 message;
    message.append(data_chunk{0x00});
    // [ COMMAND ]
    // [ ID ]
    // [ DATA ]
    // [ CHECKSUM ]
    message.append(data_chunk(command_.begin(), command_.end()));
    data_chunk raw_id = uncast_type(id_);
    BITCOIN_ASSERT(raw_id.size() == 4);
    message.append(raw_id);
    message.append(data_);
    data_chunk raw_checksum = uncast_type(generate_sha256_checksum(data_));
    BITCOIN_ASSERT(raw_checksum.size() == 4);
    message.append(raw_checksum);
    message.send(socket);
}

const uint32_t outgoing_message::id() const
{
    return id_;
}

bool incoming_message::recv(zmq::socket_t& socket)
{
    zmq::message_t message;
    // Discard empty frame.
    socket.recv(&message);
    // [ COMMAND ]
    data_chunk raw_command = next_message(socket, message);
    command_ = std::string(raw_command.begin(), raw_command.end());
    // [ ID ]
    data_chunk raw_id = next_message(socket, message);
    if (raw_id.size() != 4)
        return false;
    id_ = cast_chunk<uint32_t>(raw_id);
    // [ DATA ]
    data_ = next_message(socket, message);
    // [ CHECKSUM ]
    data_chunk raw_checksum = next_message(socket, message);
    uint32_t checksum = cast_chunk<uint32_t>(raw_checksum);
    if (checksum != generate_sha256_checksum(data_))
        return false;
    return true;
}

data_chunk incoming_message::next_message(
    zmq::socket_t& socket, zmq::message_t& message)
{
    socket.recv(&message);
    uint8_t* msg_begin = reinterpret_cast<uint8_t*>(message.data());
    return data_chunk(msg_begin, msg_begin + message.size());
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

