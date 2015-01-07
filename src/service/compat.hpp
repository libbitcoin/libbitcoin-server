#ifndef OBELISK_WORKER_SERVICE_COMPAT_HPP
#define OBELISK_WORKER_SERVICE_COMPAT_HPP

#include "util.hpp"

namespace obelisk {

class node_impl;

void COMPAT_fetch_history(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace obelisk

#endif

