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
#include <bitcoin/server/service/compat.hpp>

#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/service/fetch_x.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::chain;
using std::placeholders::_1;
using std::placeholders::_2;

struct row_pair
{
    const history_row* output = nullptr;
    uint64_t checksum;
    const history_row* spend = nullptr;
    uint64_t max_height = 0;
};

typedef std::vector<row_pair> row_pair_list;

void COMPAT_send_history_result(
    const std::error_code& ec, const history_list& history,
    const incoming_message& request, queue_send_callback queue_send,
    const uint64_t from_height);

void COMPAT_fetch_history(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    payment_address payaddr;
    uint32_t from_height;
    if (!unwrap_fetch_history_args(payaddr, from_height, request))
        return;
    // Reverse short_hash.
    uint8_t addr_version = payaddr.version();
    short_hash addr_hash = payaddr.hash();
    std::reverse(addr_hash.begin(), addr_hash.end());
    // TODO: Slows down queries!
    //log_debug(LOG_SERVICE) << "fetch_history("
    //    << payaddr.encoded() << ", from_height=" << from_height << ")";
    fetch_history(node.blockchain(), node.transaction_indexer(),
        payment_address(addr_version, addr_hash),
        std::bind(COMPAT_send_history_result,
            _1, _2, request, queue_send, from_height),
        0);
}
void COMPAT_send_history_result(
    const std::error_code& ec, const history_list& history,
    const incoming_message& request, queue_send_callback queue_send,
    const uint64_t from_height)
{
    // Create matched pairs.
    // First handle outputs.
    row_pair_list all_pairs;
    for (const history_row& row: history)
    {
        if (row.id == point_ident::output)
        {
            row_pair pair;
            pair.output = &row;
            pair.checksum = spend_checksum(row.point);
            pair.max_height = row.height;
            all_pairs.push_back(pair);
        }
    }
    // Now sort out spends.
    for (const history_row& row: history)
    {
        if (row.id == point_ident::spend)
        {
            DEBUG_ONLY(bool found = false);
            for (row_pair& pair: all_pairs)
            {
                if (pair.checksum == row.previous_checksum)
                {
                    BITCOIN_ASSERT(pair.spend == nullptr);
                    pair.spend = &row;
                    pair.max_height = row.height;
                    DEBUG_ONLY(found = true);
                    break;
                }
            }
            BITCOIN_ASSERT(found);
        }
    }
    // We have our matched pairs now.
    // Now filter out pairs we don't want.
    row_pair_list pairs;
    std::copy_if(all_pairs.begin(), all_pairs.end(),
        std::back_inserter(pairs),
        [from_height](const row_pair& pair)
        {
            return pair.max_height >= from_height;
        });
    // Free memory from all_pairs.
    all_pairs.resize(0);
    all_pairs.shrink_to_fit();
    // Now serialize data.
    constexpr size_t row_size = 36 + 4 + 8 + 36 + 4;
    data_chunk result(4 + row_size * pairs.size());
    auto serial = make_serializer(result.begin());
    write_error_code(serial, ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + 4);
    for (const row_pair& pair: pairs)
    {
        BITCOIN_ASSERT(pair.output->height <= max_uint32);
        auto output_height32 = static_cast<uint32_t>(pair.output->height);

        DEBUG_ONLY(auto start_pos = serial.iterator());
        BITCOIN_ASSERT(pair.output != nullptr);
        serial.write_hash(pair.output->point.hash);
        serial.write_4_bytes(pair.output->point.index);
        serial.write_4_bytes(output_height32);
        serial.write_8_bytes(pair.output->value);
        if (pair.spend)
        {
            BITCOIN_ASSERT(pair.spend->height <= max_uint32);
            auto spend_height32 = static_cast<uint32_t>(pair.spend->height);

            serial.write_hash(pair.spend->point.hash);
            serial.write_4_bytes(pair.spend->point.index);
            serial.write_4_bytes(spend_height32);
        }
        else
        {
            constexpr uint32_t no_value = bc::max_uint32;
            serial.write_hash(null_hash);
            serial.write_4_bytes(no_value);
            serial.write_4_bytes(no_value);
        }
        BITCOIN_ASSERT(serial.iterator() == start_pos + row_size);
    }
    BITCOIN_ASSERT(serial.iterator() == result.end());
    // TODO: Slows down queries!
    //log_debug(LOG_SERVICE)
    //    << "*.fetch_history() finished. Sending response.";
    outgoing_message response(request, result);
    queue_send(response);
}

} // namespace server
} // namespace libbitcoin


