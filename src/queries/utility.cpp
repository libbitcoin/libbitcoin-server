/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "utility.hpp"

#include <cstdint>
#include <vector>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/server/configuration.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::blockchain;
using namespace bc::wallet;

static constexpr uint8_t spend_type = 1;
static constexpr uint8_t output_type = 0;
static constexpr uint32_t no_value = bc::max_uint32;
static constexpr size_t code_size = sizeof(uint32_t);
static constexpr size_t point_size = hash_size + sizeof(uint32_t);

// fetch_history stuff
// ----------------------------------------------------------------------------
bool unwrap_fetch_history_args(payment_address& address,
    uint32_t& from_height, const incoming_message& request)
{
    static constexpr size_t history_args_size = sizeof(uint8_t) +
        short_hash_size + sizeof(uint32_t);

    const auto& data = request.data();

    if (data.size() != history_args_size)
    {
        log::error(LOG_SERVICE)
            << "Incorrect data size for .fetch_history";
        return false;
    }

    auto deserial = make_deserializer(data.begin(), data.end());
    const auto version_byte = deserial.read_byte();
    const auto hash = deserial.read_short_hash();
    from_height = deserial.read_4_bytes_little_endian();
    BITCOIN_ASSERT(deserial.iterator() == data.end());

    address = payment_address(hash, version_byte);
    return true;
}

void send_history_result(const code& ec, const block_chain::history& history,
    const incoming_message& request, send_handler handler)
{
    static constexpr size_t row_size = sizeof(uint8_t) + point_size +
        sizeof(uint32_t) + sizeof(uint64_t);

    data_chunk result(code_size + row_size * history.size());
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    for (const auto& row: history)
    {
        BITCOIN_ASSERT(row.height <= max_uint32);
        auto row_height32 = static_cast<uint32_t>(row.height);
        const auto value = row.kind == block_chain::point_kind::output ?
            output_type : spend_type;

        serial.write_byte(value);
        data_chunk raw_point = row.point.to_data();
        serial.write_data(raw_point);
        serial.write_4_bytes_little_endian(row_height32);
        serial.write_8_bytes_little_endian(row.value);
    }

    BITCOIN_ASSERT(serial.iterator() == result.end());

    outgoing_message response(request, result);
    handler(response);
}

// fetch_transaction stuff
// ----------------------------------------------------------------------------
bool unwrap_fetch_transaction_args(hash_digest& tx_hash,
    const incoming_message& request)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        log::error(LOG_SERVICE)
            << "Invalid hash length in fetch_transaction request.";
        return false;
    }

    auto deserial = make_deserializer(data.begin(), data.end());
    tx_hash = deserial.read_hash();
    return true;
}

void transaction_fetched(const code& ec, const chain::transaction& tx,
    const incoming_message& request, send_handler handler)
{
    data_chunk result(4 + tx.serialized_size());
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    data_chunk tx_data = tx.to_data();
    serial.write_data(tx_data);
    BITCOIN_ASSERT(serial.iterator() == result.end());

    outgoing_message response(request, result);
    handler(response);
}

} // namespace server
} // namespace libbitcoin
