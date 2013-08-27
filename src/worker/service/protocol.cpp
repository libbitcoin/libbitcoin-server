#include "protocol.hpp"

#include <bitcoin/satoshi_serialize.hpp>
#include "../node_impl.hpp"
#include "util.hpp"

using namespace bc;

void protocol_broadcast_transaction(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
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
        response.send(*socket);
        return;
    }
    auto ignore_send = [](const std::error_code&, size_t) {};
    // Send and hope for the best!
    node.protocol().broadcast(tx, ignore_send);
    // Response back to user saying everything is fine.
    write_error_code(serial, std::error_code());
    outgoing_message response(request, result);
    response.send(*socket);
}

