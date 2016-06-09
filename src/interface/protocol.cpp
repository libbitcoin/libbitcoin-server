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
#include <bitcoin/server/interface/protocol.hpp>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <bitcoin/server.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;

// This does NOT save to our memory pool.
// The transaction will hit our memory pool when it is picked up from a peer.
void protocol::broadcast_transaction(server_node& node,
    const incoming& request, send_handler handler)
{
    chain::transaction tx;

    if (!tx.from_data(request.data))
    {
        handler(outgoing(request, error::bad_stream));
        return;
    }

    const auto ignore_complete = [](const code&) {};
    const auto ignore_send = [](const code&, network::channel::ptr) {};

    // Send and hope for the best!
    node.broadcast(tx, ignore_send, ignore_complete);

    // Tell the user everything is fine.
    handler(outgoing(request, error::success));
}

void protocol::total_connections(server_node& node, const incoming& request,
    send_handler handler)
{
    if (!request.data.empty())
    {
        handler(outgoing(request, error::bad_stream));
        return;
    }

    node.connected_count(
        std::bind(&protocol::handle_total_connections,
            _1, request, handler));
}

void protocol::handle_total_connections(size_t count, const incoming& request,
    send_handler handler)
{
    BITCOIN_ASSERT(count <= max_uint32);
    const auto total_connections = static_cast<uint32_t>(count);

    data_chunk result(code_size + sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    serial.write_error_code(error::success);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    serial.write_4_bytes_little_endian(total_connections);
    BITCOIN_ASSERT(serial.iterator() == result.end());

    handler(outgoing(request, result));
}

} // namespace server
} // namespace libbitcoin
