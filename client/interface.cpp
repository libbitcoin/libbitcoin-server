#include "interface.hpp"

using namespace bc;
using std::placeholders::_1;

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
    BITCOIN_ASSERT(std::distance(data.begin(), serial.iterator()) ==
        1 + short_hash_size);
    backend_.request("blockchain.fetch_history", data,
        std::bind(wrap_fetch_history, _1, handle_fetch));
}
void wrap_fetch_history(const data_chunk& data,
    blockchain::fetch_handler_history handle_fetch)
{
    size_t row_size = 36 + 4 + 8 + 36 + 4;
    if (data.size() % row_size != 0)
    {
        log_error() << "Malformed response for blockchain.fetch_history";
        return;
    }
    std::error_code ec;
    blockchain::history_list history(data.size() / row_size);
    auto deserial = make_deserializer(data.begin(), data.end());
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

fullnode_interface::fullnode_interface()
  : blockchain(backend_)
{
}
void fullnode_interface::update()
{
    backend_.update();
}

