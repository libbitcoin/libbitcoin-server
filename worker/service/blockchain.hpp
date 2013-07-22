#ifndef OBELISK_WORKER_SERVICE_BLOCKCHAIN_HPP
#define OBELISK_WORKER_SERVICE_BLOCKCHAIN_HPP

#include <obelisk/message.hpp>

class node_impl;

void blockchain_fetch_history(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

// fetch_tx

// fetch_last_depth

// fetch_block_header

#endif

