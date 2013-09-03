#ifndef OBELISK_WORKER_SERVICE_BLOCKCHAIN_HPP
#define OBELISK_WORKER_SERVICE_BLOCKCHAIN_HPP

#include <obelisk/message.hpp>

namespace obelisk {

class node_impl;

void blockchain_fetch_history(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

void blockchain_fetch_transaction(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

void blockchain_fetch_last_height(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

void blockchain_fetch_block_header(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

} // namespace obelisk

#endif

