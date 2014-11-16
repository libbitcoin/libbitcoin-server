#ifndef OBELISK_WORKER_SERVICE_FULLNODE_HPP
#define OBELISK_WORKER_SERVICE_FULLNODE_HPP

#include <obelisk/define.hpp>
#include <obelisk/message.hpp>
#include "util.hpp"

namespace obelisk {

class node_impl;

void fullnode_fetch_history(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace obelisk

#endif

