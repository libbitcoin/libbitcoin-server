#ifndef OBELISK_WORKER_SERVICE_TRANSACTION_POOL_HPP
#define OBELISK_WORKER_SERVICE_TRANSACTION_POOL_HPP

#include <obelisk/message.hpp>

class node_impl;

void transaction_pool_validate(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

#endif

