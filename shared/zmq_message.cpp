#include "zmq_message.hpp"

#include <bitcoin/format.hpp>
#include <bitcoin/utility/assert.hpp>

using namespace bc;

void zmq_message::append(const data_chunk& part)
{
    parts_.push_back(part);
}

void zmq_message::send(zmq::socket_t& socket)
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

bool zmq_message::recv(zmq::socket_t& socket)
{
    parts_.clear();
    int64_t more = 1;
    while (more)
    {
        zmq::message_t message(0);
        try
        {
            if (!socket.recv(&message, 0))
                return false;
        }
        catch (zmq::error_t error)
        {
            return false;
        }
        uint8_t* msg_begin = reinterpret_cast<uint8_t*>(message.data());
        data_chunk data(msg_begin, msg_begin + message.size());
        parts_.push_back(data);
        size_t more_size = sizeof (more);
        socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
    }
    return true;
}

const zmq_message::data_stack& zmq_message::parts() const
{
    return parts_;
}

