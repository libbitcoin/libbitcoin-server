#include "subscribe_manager.hpp"

#include "service/util.hpp"

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;
namespace posix_time = boost::posix_time;
using posix_time::minutes;
using posix_time::second_clock;

const posix_time::time_duration sub_expiry = minutes(20);

subscribe_manager::subscribe_manager(node_impl& node)
  : strand_(node.memory_related_threadpool())
{
    // subscribe to blocks and txs -> submit
}

void subscribe_manager::subscribe(
    const incoming_message& request, zmq_socket_ptr socket)
{
    strand_.queue(std::bind(&subscribe_manager::do_subscribe,
        this, request, socket));
}
void subscribe_manager::do_subscribe(
    const incoming_message& request, zmq_socket_ptr socket)
{
    // Read address.
    payment_address addr_key;
    const data_chunk& data = request.data();
    auto deserial = make_deserializer(data.begin(), data.end());
    uint8_t version_byte = deserial.read_byte();
    short_hash hash = deserial.read_short_hash();
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    addr_key.set(version_byte, hash);
    // Now create subscription.
    const posix_time::ptime now = second_clock::universal_time();
    subs_.emplace(addr_key, subscription{
        now + sub_expiry, request.origin(), socket});
    // Send response.
    data_chunk result(4);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, std::error_code());
    outgoing_message response(request, result);
    response.send(*socket);
}

