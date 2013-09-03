#include "fetch_history.hpp"

#include "../echo.hpp"
#include "util.hpp"

namespace obelisk {

using namespace bc;

bool unwrap_fetch_history_args(
    payment_address& payaddr, uint32_t& from_height,
    const incoming_message& request)
{
    const data_chunk& data = request.data();
    if (data.size() != 1 + short_hash_size + 4)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for .fetch_history";
        return false;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    uint8_t version_byte = deserial.read_byte();
    short_hash hash = deserial.read_short_hash();
    from_height = deserial.read_4_bytes();
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    payaddr.set(version_byte, hash);
    return true;
}
bool send_history_result(const std::error_code& ec,
    const blockchain::history_list& history,
    const incoming_message& request, zmq_socket_ptr socket)
{
    size_t row_size = 36 + 4 + 8 + 36 + 4;
    data_chunk result(4 + row_size * history.size());
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    for (const blockchain::history_row row: history)
    {
        serial.write_hash(row.output.hash);
        serial.write_4_bytes(row.output.index);
        serial.write_4_bytes(row.output_height);
        serial.write_8_bytes(row.value);
        serial.write_hash(row.spend.hash);
        serial.write_4_bytes(row.spend.index);
        serial.write_4_bytes(row.spend_height);
    }
    BITCOIN_ASSERT(serial.iterator() == result.end());
    outgoing_message response(request, result);
    log_debug(LOG_WORKER)
        << "*.fetch_history() finished. Sending response.";
    response.send(*socket);
}

} // namespace obelisk

