#ifndef OBELISK_WORKER_SUBSCRIBE_MANAGER_HPP
#define OBELISK_WORKER_SUBSCRIBE_MANAGER_HPP

#include <obelisk/message.hpp>
#include "node_impl.hpp"

class subscribe_manager
{
public:
    subscribe_manager(node_impl& node);
    void subscribe(const incoming_message& request, zmq_socket_ptr socket);
};

#endif

