#ifndef OBELISK_WORKER_SERVICE_FETCH_HISTORY_HPP
#define OBELISK_WORKER_SERVICE_FETCH_HISTORY_HPP

#include <obelisk/message.hpp>
#include "../node_impl.hpp"
#include "util.hpp"

namespace obelisk {

bool unwrap_fetch_history_args(
    bc::payment_address& payaddr, uint32_t& from_height,
    const incoming_message& request);

bool send_history_result(const std::error_code& ec,
    const bc::blockchain::history_list& history,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace obelisk

#endif

