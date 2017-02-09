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
#include <bitcoin/server/interface/address.hpp>

#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::wallet;

// Transaction pool address history is not indexed.
////void address::fetch_history2(server_node& node, const message& request,
////    send_handler handler)
////{
////    static constexpr size_t limit = 0;
////    size_t from_height;
////    payment_address address;
////
////    if (!unwrap_fetch_history_args(address, from_height, request))
////    {
////        handler(message(request, error::bad_stream));
////        return;
////    }
////
////    // Obtain payment address history from the transaction pool and blockchain.
////    node.chain().fetch_history(address, limit, from_height,
////        std::bind(send_history_result,
////            _1, _2, request, handler));
////}

// v3 eliminates the subscription type, which we map to 'unspecified'.
void address::subscribe2(server_node& node, const message& request,
    send_handler handler)
{
    binary prefix_filter;

    if (!unwrap_subscribe2_args(prefix_filter, request))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    node.subscribe_address(request.route(), request.id(), prefix_filter, false);
    handler(message(request, error::success));
}

// v3 adds unsubscribe2, which we map to subscription_type 'unsubscribe'.
void address::unsubscribe2(server_node& node, const message& request,
    send_handler handler)
{
    binary prefix_filter;

    if (!unwrap_subscribe2_args(prefix_filter, request))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    node.subscribe_address(request.route(), request.id(), prefix_filter, true);
    handler(message(request, error::success));
}

bool address::unwrap_subscribe2_args(binary& prefix_filter,
    const message& request)
{
    // [ prefix_bitsize:1 ]
    // [ prefix_blocks:...]
    const auto& data = request.data();

    if (data.empty())
        return false;

    // First byte is the number of bits.
    auto bit_length = data[0];

    //// The max byte value is 255, so this is unnecessary.
    ////static constexpr size_t address_bits = hash_size * byte_bits;
    ////if (bit_length > address_bits)
    ////    return false;

    // Convert the bit length to byte length.
    const auto byte_length = binary::blocks_size(bit_length);

    if (data.size() - 1 != byte_length)
        return false;

    const data_chunk bytes({ data.begin() + 1, data.end() });
    prefix_filter = binary(bit_length, bytes);
    return true;
}

} // namespace server
} // namespace libbitcoin
