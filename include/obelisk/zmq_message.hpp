#ifndef OBELISK_ZMQ_MESSAGE
#define OBELISK_ZMQ_MESSAGE

#include <zmq.hpp>
#include <bitcoin/types.hpp>
namespace bc = libbitcoin;

class zmq_message
{
public:
    void append(const bc::data_chunk& part);
    void send(zmq::socket_t& socket) const;
    bool recv(zmq::socket_t& socket);
    const bc::data_stack& parts() const;

private:
    bc::data_stack parts_;
};

#endif

