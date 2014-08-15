#ifndef OBELISK_WORKER_CLIENT_FETCH_X_HPP
#define OBELISK_WORKER_CLIENT_FETCH_X_HPP

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <obelisk/message.hpp>

namespace obelisk {

// fetch_history stuff

void wrap_fetch_history_args(bc::data_chunk& data,
    const bc::payment_address& address, size_t from_height);

void receive_history_result(const bc::data_chunk& data,
    bc::blockchain::fetch_handler_history handle_fetch);

// fetch_transaction stuff

void wrap_fetch_transaction_args(bc::data_chunk& data, 
    const bc::hash_digest& tx_hash);

void receive_transaction_result(const bc::data_chunk& data,
    bc::blockchain::fetch_handler_transaction handle_fetch);

} // namespace obelisk

#endif

