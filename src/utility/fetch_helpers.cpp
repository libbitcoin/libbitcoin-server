/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/utility/fetch_helpers.hpp>

#include <cstddef>
#include <cstdint>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/message.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::wallet;

// fetch_history stuff
// ----------------------------------------------------------------------------

bool unwrap_fetch_history_args(payment_address& address,
    size_t& from_height, const message& request)
{
    static constexpr size_t history_args_size = sizeof(uint8_t) +
        short_hash_size + sizeof(uint32_t);

    const auto& data = request.data();

    if (data.size() != history_args_size)
    {
        LOG_ERROR(LOG_SERVER)
            << "Incorrect data size for .fetch_history";
        return false;
    }

    // TODO: add serialization to history_compact.
    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto version_byte = deserial.read_byte();
    const auto hash = deserial.read_short_hash();
    from_height = static_cast<size_t>(deserial.read_4_bytes_little_endian());
    ////BITCOIN_ASSERT(deserial.iterator() == data.end());

    address = payment_address(hash, version_byte);
    return true;
}

void send_history_result(const code& ec,
    const chain::history_compact::list& history, const message& request,
    send_handler handler)
{
    static constexpr size_t row_size = sizeof(uint8_t) + point_size +
        sizeof(uint32_t) + sizeof(uint64_t);

    data_chunk result(code_size + row_size * history.size());
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);
    ////BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    // TODO: add serialization to history_compact.
    for (const auto& row: history)
    {
        BITCOIN_ASSERT(row.height <= max_uint32);
        serial.write_byte(static_cast<uint8_t>(row.kind));
        serial.write_bytes(row.point.to_data());
        serial.write_4_bytes_little_endian(row.height);
        serial.write_8_bytes_little_endian(row.value);
    }

    ////BITCOIN_ASSERT(serial.iterator() == result.end());

    handler(message(request, result));
}

// fetch_transaction stuff
// ----------------------------------------------------------------------------

bool unwrap_fetch_transaction_args(hash_digest& hash, const message& request)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        LOG_ERROR(LOG_SERVER)
            << "Invalid hash length in fetch_transaction request.";
        return false;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    hash = deserial.read_hash();
    return true;
}

void transaction_fetched(const code& ec, transaction_ptr tx, size_t, size_t,
    const message& request, send_handler handler)
{
    const auto result = build_chunk(
    {
        message::to_bytes(ec),
        tx->to_data()
    });

    handler(message(request, result));
}

} // namespace server
} // namespace libbitcoin
