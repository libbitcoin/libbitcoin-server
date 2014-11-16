#ifndef OBELISK_WORKER_SERVICE_BLOCKCHAIN_HPP
#define OBELISK_WORKER_SERVICE_BLOCKCHAIN_HPP

#include <obelisk/define.hpp>
#include "util.hpp"

namespace obelisk {

class node_impl;

void blockchain_fetch_history(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_transaction(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_last_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_block_header(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_block_transaction_hashes(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_transaction_index(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_spend(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_block_height(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

void blockchain_fetch_stealth(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace obelisk

#endif

