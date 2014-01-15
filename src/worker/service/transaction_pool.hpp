#ifndef OBELISK_WORKER_SERVICE_TRANSACTION_POOL_HPP
#define OBELISK_WORKER_SERVICE_TRANSACTION_POOL_HPP

#include <obelisk/message.hpp>
#include "util.hpp"

namespace obelisk {

class node_impl;

void transaction_pool_validate(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void transaction_pool_fetch_transaction(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace obelisk

#endif

