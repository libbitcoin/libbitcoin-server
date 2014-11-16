#include "../echo.hpp"
#include "fetch_x.hpp"
#include "util.hpp"

namespace obelisk {

using namespace bc;

// fetch_history stuff

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
void send_history_result(
    const std::error_code& ec, const history_list& history,
    const incoming_message& request, queue_send_callback queue_send)
{
    constexpr size_t row_size = 1 + 36 + 4 + 8;
    data_chunk result(4 + row_size * history.size());
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    for (const history_row& row: history)
    {
        if (row.id == point_ident::output)
            serial.write_byte(0);
        else // if (row.id == point_ident::spend)
            serial.write_byte(1);
        serial.write_hash(row.point.hash);
        serial.write_4_bytes(row.point.index);
        serial.write_4_bytes(row.height);
        serial.write_8_bytes(row.value);
    }
    BITCOIN_ASSERT(serial.iterator() == result.end());
    // TODO: Slows down queries!
    //log_debug(LOG_WORKER)
    //    << "*.fetch_history() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

// fetch_transaction stuff

bool unwrap_fetch_transaction_args(
    hash_digest& tx_hash, const incoming_message& request)
{
    const data_chunk& data = request.data();
    if (data.size() != 32)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for *.fetch_transaction";
        return false;
    }
    auto deserial = make_deserializer(data.begin(), data.end());
    tx_hash = deserial.read_hash();
    return true;
}

void transaction_fetched(const std::error_code& ec,
    const transaction_type& tx,
    const incoming_message& request, queue_send_callback queue_send)
{
    data_chunk result(4 + satoshi_raw_size(tx));
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    auto it = satoshi_save(tx, serial.iterator());
    BITCOIN_ASSERT(it == result.end());
    log_debug(LOG_REQUEST)
        << "blockchain.fetch_transaction() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

} // namespace obelisk

