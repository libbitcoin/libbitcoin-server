#ifndef LIBBITCOIN_SERVER_COMPAT_FETCH_HISTORY_HPP
#define LIBBITCOIN_SERVER_COMPAT_FETCH_HISTORY_HPP

#include "util.hpp"

namespace server {

class node_impl;

void COMPAT_fetch_history(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace server

#endif

