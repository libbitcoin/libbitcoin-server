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
#include <bitcoin/server/interface/protocol.hpp>

#include <cstdint>
#include <bitcoin/server.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;

void protocol::total_connections(server_node& node, const message& request,
    send_handler handler)
{
    if (!request.data().empty())
    {
        handler(message(request, error::bad_stream));
        return;
    }

    const auto count = node.connection_count();
    BITCOIN_ASSERT(count <= max_uint32);

    // [ code:4 ]
    // [ connections:4 ]
    const auto result = build_chunk(
    {
        message::to_bytes(error::success),
        to_little_endian(static_cast<uint32_t>(count))
    });

    handler(message(request, result));
}

////// This does NOT save to our tx pool.
////// The transaction will hit our memory pool when it is picked up from a peer.
////void protocol::broadcast_transaction(server_node& node, const message& request,
////    send_handler handler)
////{
////    static const auto version = bc::message::version::level::canonical;
////    bc::message::transaction tx;
////
////    if (!tx.from_data(version, request.data()))
////    {
////        handler(message(request, error::bad_stream));
////        return;
////    }
////
////    const auto ignore_complete = [](const code&) {};
////    const auto ignore_send = [](const code&, network::channel::ptr) {};
////
////    // Send and hope for the best!
////    node.broadcast(tx, ignore_send, ignore_complete);
////
////    // Tell the user everything is fine.
////    handler(message(request, error::success));
////}

} // namespace server
} // namespace libbitcoin
