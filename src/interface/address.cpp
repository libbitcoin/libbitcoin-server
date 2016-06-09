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
#include <bitcoin/server/interface/address.hpp>

#include <cstdint>
#include <functional>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::wallet;

void address::fetch_history2(server_node& node, const incoming& request,
    send_handler handler)
{
    static constexpr uint64_t limit = 0;
    uint32_t from_height;
    payment_address address;

    if (!unwrap_fetch_history_args(address, from_height, request))
    {
        handler(outgoing(request, error::bad_stream));
        return;
    }

    // Obtain payment address history from the transaction pool and blockchain.
    node.pool().fetch_history(address, limit, from_height,
        std::bind(send_history_result,
            _1, _2, request, handler));
}

void address::subscribe(server_node& node, const incoming& request,
    send_handler handler)
{
    route reply_to;
    binary prefix_filter;
    subscribe_type type;

    if (!unwrap_subscribe_args(reply_to, prefix_filter, type, request))
    {
        handler(outgoing(request, error::bad_stream));
        return;
    }

    // TODO: reenable.
    ////node.subscribe_address(reply_to, prefix_filter, type);

    handler(outgoing(request, error::success));
}

bool address::unwrap_subscribe_args(route& reply_to, binary& prefix_filter, 
    subscribe_type& type, const incoming& request)
{
    // [ type:1 ] (0 = address prefix, 1 = stealth prefix)
    // [ prefix_bitsize:1 ]
    // [ prefix_blocks:...  ]
    const auto& data = request.data;

    if (data.size() < 2)
        return false;

    // First byte is the subscribe_type enumeration.
    if (data[0] != static_cast<uint8_t>(subscribe_type::address) &&
        data[0] != static_cast<uint8_t>(subscribe_type::stealth))
        return false;

    type = static_cast<subscribe_type>(data[0]);

    // Second byte is the number of bits.
    const auto bit_length = data[1];

    // Convert the bit length to byte length.
    const auto byte_length = binary::blocks_size(bit_length);

    if (data.size() != byte_length + 2)
        return false;

    const data_chunk bytes({ data.begin() + 2, data.end() });
    prefix_filter = binary(bit_length, bytes);
    return true;
}

////void address::renew(server_node& node, const incoming& request,
////    send_handler handler)
////{
////    route reply_to;
////    binary prefix_filter;
////    subscribe_type type;
////
////    if (!unwrap_subscribe_args(reply_to, prefix_filter, type, request))
////    {
////        handler(outgoing(request, error::bad_stream));
////        return;
////    }
////
////    node.renew(reply_to, prefix_filter, type);
////}
////
////bool address::unwrap_renew_args(route& reply_to, binary& prefix_filter,
////    subscribe_type& type, const incoming& request)
////{
////    // These are currently isomorphic.
////    return unwrap_subscribe_args(reply_to, prefix_filter, type, request);
////}

} // namespace server
} // namespace libbitcoin
