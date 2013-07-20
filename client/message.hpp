#ifndef OBELISK_CLIENT_MESSAGE
#define OBELISK_CLIENT_MESSAGE

#include <zmq.hpp>
#include <bitcoin/types.hpp>
namespace bc = libbitcoin;

class incoming_message
{
public:
    bool recv(zmq::socket_t& socket);

    const std::string& command() const;
    const uint32_t id() const;
    const bc::data_chunk& data() const;

private:
    bc::data_chunk next_message(zmq::socket_t& socket, zmq::message_t& message);

    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

class outgoing_message
{
public:
    outgoing_message(const std::string& command, const bc::data_chunk& data);
    outgoing_message(
        const incoming_message& request, const bc::data_chunk& data);
    // Default constructor provided for containers and copying.
    outgoing_message();

    void send(zmq::socket_t& socket);
    const uint32_t id() const;

private:
    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

#endif

