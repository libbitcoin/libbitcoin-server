#ifndef OBELISK_ZMQ_MESSAGE
#define OBELISK_ZMQ_MESSAGE

#include <zmq.hpp>
#include <bitcoin/types.hpp>
namespace bc = libbitcoin;

class zmq_message
{
public:
    typedef std::vector<bc::data_chunk> data_stack;

    void append(const bc::data_chunk& part);
    void send(zmq::socket_t& socket);
    bool recv(zmq::socket_t& socket);
    const data_stack& parts() const;

private:
    data_stack parts_;
};

#endif

