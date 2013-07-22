#include <obelisk/client/blockchain.hpp>

using namespace bc;
using std::placeholders::_1;

template <typename Deserializer>
bool read_error_code(Deserializer& deserial,
    size_t data_size, std::error_code& ec)
{
    if (data_size < 4)
    {
        log_error() << "No error_code in response.";
        return false;
    }
    uint32_t val = deserial.read_4_bytes();
    ec = static_cast<error::error_code_t>(val);
    return true;
}

blockchain_interface::blockchain_interface(backend_cluster& backend)
  : backend_(backend)
{
}

void wrap_fetch_history(const data_chunk& data,
    blockchain::fetch_handler_history handle_fetch);
void blockchain_interface::fetch_history(const payment_address& address,
    blockchain::fetch_handler_history handle_fetch)
{
    data_chunk data(1 + short_hash_size);
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    BITCOIN_ASSERT(serial.iterator() == data.end());
    backend_.request("blockchain.fetch_history", data,
        std::bind(wrap_fetch_history, _1, handle_fetch));
}
void wrap_fetch_history(const data_chunk& data,
    blockchain::fetch_handler_history handle_fetch)
{
    std::error_code ec;
    auto deserial = make_deserializer(data.begin(), data.end());
    if (!read_error_code(deserial, data.size(), ec))
        return;
    BITCOIN_ASSERT(deserial.iterator() == data.begin() + 4);
    size_t row_size = 36 + 4 + 8 + 36 + 4;
    BITCOIN_ASSERT(data.size() >= 4);
    if ((data.size() - 4) % row_size != 0)
    {
        log_error() << "Malformed response for blockchain.fetch_history";
        return;
    }
    size_t number_rows = (data.size() - 4) / row_size;
    blockchain::history_list history(number_rows);
    for (size_t i = 0; i < history.size(); ++i)
    {
        blockchain::history_row& row = history[i];
        row.output.hash = deserial.read_hash();
        row.output.index = deserial.read_4_bytes();
        row.output_height = deserial.read_4_bytes();
        row.value = deserial.read_8_bytes();
        row.spend.hash = deserial.read_hash();
        row.spend.index = deserial.read_4_bytes();
        row.spend_height = deserial.read_4_bytes();
    }
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    handle_fetch(ec, history);
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

