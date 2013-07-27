#ifndef OBELISK_WORKER_CLIENT_FETCH_HISTORY_HPP
#define OBELISK_WORKER_CLIENT_FETCH_HISTORY_HPP

#include <obelisk/message.hpp>
#include <bitcoin/bitcoin.hpp>

using namespace bc;

void wrap_fetch_history_args(data_chunk& data,
    const payment_address& address, size_t from_height);

void receive_history_result(const data_chunk& data,
    blockchain::fetch_handler_history handle_fetch);

#endif

