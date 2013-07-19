#include "interface.hpp"

using namespace bc;

blockchain_interface::blockchain_interface(backend_cluster& backend)
  : backend_(backend)
{
}

void blockchain_interface::fetch_history(const bc::payment_address& address,
    bc::blockchain::fetch_handler_history handle_fetch)
{
    data_chunk data(1 + short_hash_size);
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    BITCOIN_ASSERT(std::distance(data.begin(), serial.iterator()) ==
        1 + short_hash_size);
    auto wrap_handler = [handle_fetch](const data_chunk& data)
        {
            std::error_code ec;
            blockchain::history_list history;
            handle_fetch(ec, history);
        };
    backend_.request("blockchain.fetch_history", data, wrap_handler);
}

fullnode_interface::fullnode_interface()
  : blockchain(backend_)
{
}
void fullnode_interface::update()
{
    backend_.update();
}

