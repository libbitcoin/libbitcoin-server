#include <obelisk/client/blockchain.hpp>

#include "fetch_history.hpp"
#include "util.hpp"

namespace obelisk {

using namespace bc;
using std::placeholders::_1;

blockchain_interface::blockchain_interface(backend_cluster& backend)
  : backend_(backend)
{
}

void blockchain_interface::fetch_history(const payment_address& address,
    blockchain::fetch_handler_history handle_fetch, size_t from_height)
{
    data_chunk data;
    wrap_fetch_history_args(data, address, from_height);
    backend_.request("blockchain.fetch_history", data,
        std::bind(receive_history_result, _1, handle_fetch));
}

void wrap_fetch_transaction(const data_chunk& data,
    blockchain::fetch_handler_transaction handle_fetch);
void blockchain_interface::fetch_transaction(const hash_digest& tx_hash,
    blockchain::fetch_handler_transaction handle_fetch)
{
    data_chunk data(hash_digest_size);
    auto serial = make_serializer(data.begin());
    serial.write_hash(tx_hash);
    BITCOIN_ASSERT(serial.iterator() == data.end());
    backend_.request("blockchain.fetch_transaction", data,
        std::bind(wrap_fetch_transaction, _1, handle_fetch));
}
void wrap_fetch_transaction(const data_chunk& data,
    blockchain::fetch_handler_transaction handle_fetch)
{
    std::error_code ec;
    auto deserial = make_deserializer(data.begin(), data.end());
    if (!read_error_code(deserial, data.size(), ec))
        return;
    BITCOIN_ASSERT(deserial.iterator() == data.begin() + 4);
    transaction_type tx;
    satoshi_load(deserial.iterator(), data.end(), tx);
    handle_fetch(ec, tx);
}

void wrap_fetch_last_height(const data_chunk& data,
    blockchain::fetch_handler_last_height handle_fetch);
void blockchain_interface::fetch_last_height(
    blockchain::fetch_handler_last_height handle_fetch)
{
    backend_.request("blockchain.fetch_last_height", data_chunk(),
        std::bind(wrap_fetch_last_height, _1, handle_fetch));
}
void wrap_fetch_last_height(const data_chunk& data,
    blockchain::fetch_handler_last_height handle_fetch)
{
    if (data.size() != 8)
    {
        log_error() << "Malformed response for blockchain.fetch_history";
        return;
    }
    std::error_code ec;
    auto deserial = make_deserializer(data.begin(), data.end());
    bool read_success = read_error_code(deserial, data.size(), ec);
    BITCOIN_ASSERT(read_success);
    size_t last_height = deserial.read_4_bytes();
    handle_fetch(ec, last_height);
}

void wrap_fetch_block_header(const data_chunk& data,
    blockchain::fetch_handler_block_header handle_fetch);
void blockchain_interface::fetch_block_header(size_t height,
    bc::blockchain::fetch_handler_block_header handle_fetch)
{
    data_chunk data = uncast_type<uint32_t>(height);
    backend_.request("blockchain.fetch_block_header", data,
        std::bind(wrap_fetch_block_header, _1, handle_fetch));
}
void blockchain_interface::fetch_block_header(const hash_digest& blk_hash,
    blockchain::fetch_handler_block_header handle_fetch)
{
    data_chunk data(hash_digest_size);
    auto serial = make_serializer(data.begin());
    serial.write_hash(blk_hash);
    BITCOIN_ASSERT(serial.iterator() == data.end());
    backend_.request("blockchain.fetch_block_header", data,
        std::bind(wrap_fetch_block_header, _1, handle_fetch));
}
void wrap_fetch_block_header(const data_chunk& data,
    blockchain::fetch_handler_block_header handle_fetch)
{
    std::error_code ec;
    auto deserial = make_deserializer(data.begin(), data.end());
    if (!read_error_code(deserial, data.size(), ec))
        return;
    BITCOIN_ASSERT(deserial.iterator() == data.begin() + 4);
    block_header_type blk;
    satoshi_load(deserial.iterator(), data.end(), blk);
    handle_fetch(ec, blk);
}

} // namespace obelisk

