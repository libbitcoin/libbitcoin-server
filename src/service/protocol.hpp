#ifndef OBELISK_WORKER_SERVICE_PROTOCOL_HPP
#define OBELISK_WORKER_SERVICE_PROTOCOL_HPP

#include <obelisk/define.hpp>
#include <obelisk/message.hpp>
#include "util.hpp"

namespace obelisk {

class node_impl;

void protocol_broadcast_transaction(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace obelisk

#endif

