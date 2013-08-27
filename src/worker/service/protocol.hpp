#ifndef OBELISK_WORKER_SERVICE_PROTOCOL_HPP
#define OBELISK_WORKER_SERVICE_PROTOCOL_HPP

#include <obelisk/message.hpp>

class node_impl;

void protocol_broadcast_transaction(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

#endif

