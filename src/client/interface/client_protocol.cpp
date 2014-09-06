#include <obelisk/client/protocol.hpp>
#include "client_util.hpp"

namespace obelisk {

using namespace bc;
using std::placeholders::_1;

protocol_interface::protocol_interface(backend_cluster& backend)
  : backend_(backend)
{
}

void wrap_broadcast_transaction(const data_chunk& data,
    protocol_interface::broadcast_handler handle_broadcast);
void protocol_interface::broadcast_transaction(const transaction_type& tx,
    broadcast_handler handle_broadcast)
{
    data_chunk data(satoshi_raw_size(tx));
    auto it = satoshi_save(tx, data.begin());
    BITCOIN_ASSERT(it == data.end());
    backend_.request("protocol.broadcast_transaction", data,
        std::bind(wrap_broadcast_transaction, _1, handle_broadcast));
}
void wrap_broadcast_transaction(const data_chunk& data,
    protocol_interface::broadcast_handler handle_broadcast)
{
    BITCOIN_ASSERT(data.size() == 4);
    std::error_code ec;
    auto deserial = make_deserializer(data.begin(), data.end());
    if (!read_error_code(deserial, data.size(), ec))
        return;
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    handle_broadcast(ec);
}

} // namespace obelisk

