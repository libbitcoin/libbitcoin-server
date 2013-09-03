#ifndef OBELISK_WORKER_SERVICE_FULLNODE_HPP
#define OBELISK_WORKER_SERVICE_FULLNODE_HPP

#include <obelisk/message.hpp>

namespace obelisk {

class node_impl;

void fullnode_fetch_history(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket);

} // namespace obelisk

#endif

