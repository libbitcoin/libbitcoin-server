#ifndef OBELISK_WORKER_CLIENT_FETCH_X_HPP
#define OBELISK_WORKER_CLIENT_FETCH_X_HPP

#include <bitcoin/bitcoin.hpp>
#include <obelisk/message.hpp>

namespace obelisk {

using namespace bc;

// fetch_history stuff

void wrap_fetch_history_args(data_chunk& data,
    const payment_address& address, size_t from_height);

void receive_history_result(const data_chunk& data,
    blockchain::fetch_handler_history handle_fetch);

// fetch_transaction stuff

void wrap_fetch_transaction_args(data_chunk& data, const hash_digest& tx_hash);

void receive_transaction_result(const data_chunk& data,
    blockchain::fetch_handler_transaction handle_fetch);

} // namespace obelisk

#endif

