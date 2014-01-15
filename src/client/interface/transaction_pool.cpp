#include <obelisk/client/transaction_pool.hpp>

#include "fetch_x.hpp"
#include "util.hpp"

namespace obelisk {

using namespace bc;
using std::placeholders::_1;

transaction_pool_interface::transaction_pool_interface(backend_cluster& backend)
  : backend_(backend)
{
}

void wrap_validate_transaction(const data_chunk& data,
    transaction_pool::validate_handler handle_validate);
void transaction_pool_interface::validate(const transaction_type& tx,
    transaction_pool::validate_handler handle_validate)
{
    data_chunk raw_tx(satoshi_raw_size(tx));
    auto it = satoshi_save(tx, raw_tx.begin());
    BITCOIN_ASSERT(it == raw_tx.end());
    backend_.request("transaction_pool.validate", raw_tx,
        std::bind(wrap_validate_transaction, _1, handle_validate));
}
void wrap_validate_transaction(const data_chunk& data,
    bc::transaction_pool::validate_handler handle_validate)
{
    std::error_code ec;
    auto deserial = make_deserializer(data.begin(), data.end());
    if (!read_error_code(deserial, data.size(), ec))
        return;
    BITCOIN_ASSERT(deserial.iterator() == data.begin() + 4);
    index_list unconfirmed;
    size_t unconfirmed_size = (data.size() - 4) / 4;
    for (size_t i = 0; i < unconfirmed_size; ++i)
    {
        size_t unconfirm_index = deserial.read_4_bytes();
        unconfirmed.push_back(unconfirm_index);
    }
    handle_validate(ec, unconfirmed);
}

void transaction_pool_interface::fetch_transaction(
    const bc::hash_digest& tx_hash,
    transaction_pool::fetch_handler handle_fetch)
{
    data_chunk data;
    wrap_fetch_transaction_args(data, tx_hash);
    backend_.request("transaction_pool.fetch_transaction", data,
        std::bind(receive_transaction_result, _1, handle_fetch));
}

} // namespace obelisk

