#include "subscribe_manager.hpp"

using namespace bc;

subscribe_manager::subscribe_manager(node_impl& node)
{
    // subscribe to blocks and txs
}

void subscribe_manager::subscribe(
    const incoming_message& request, zmq_socket_ptr socket)
{
    outgoing_message response(request, data_chunk());
    response.send(*socket);
}

