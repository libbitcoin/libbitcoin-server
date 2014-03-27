#include "protocol.hpp"

#include <bitcoin/satoshi_serialize.hpp>
#include "../node_impl.hpp"
#include "../echo.hpp"
#include "util.hpp"

namespace obelisk {

using namespace bc;

void protocol_broadcast_transaction(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& raw_tx = request.data();
    transaction_type tx;
    data_chunk result(4);
    auto serial = make_serializer(result.begin());
    try
    {
        satoshi_load(raw_tx.begin(), raw_tx.end(), tx);
    }
    catch (end_of_stream)
    {
        // error
        write_error_code(serial, error::bad_stream);
        outgoing_message response(request, result);
        queue_send(response);
        return;
    }
    auto ignore_send = [](const std::error_code&, size_t) {};
    // Send and hope for the best!
    node.protocol().broadcast(tx, ignore_send);
    // Response back to user saying everything is fine.
    write_error_code(serial, std::error_code());
    log_debug(LOG_WORKER)
        << "protocol.broadcast_transaction() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

} // namespace obelisk

