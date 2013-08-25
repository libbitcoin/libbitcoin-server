#include "fetch_history.hpp"

#include "util.hpp"

void wrap_fetch_history_args(data_chunk& data,
    const payment_address& address, size_t from_height)
{
    data.resize(1 + short_hash_size + 4);
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    serial.write_4_bytes(from_height);
    BITCOIN_ASSERT(serial.iterator() == data.end());
}

void receive_history_result(const data_chunk& data, const worker_uuid& worker,
    address_subscriber::fetch_handler_history handle_fetch)
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
        log_error() << "Malformed response for *.fetch_history";
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
    handle_fetch(ec, history, worker);
}

