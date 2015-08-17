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
#include <bitcoin/server/service/protocol.hpp>

#include <bitcoin/server.hpp>
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

void protocol_broadcast_transaction(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    const data_chunk& raw_tx = request.data();
    chain::transaction tx;
    data_chunk result(4);
    auto serial = make_serializer(result.begin());

    if (!tx.from_data(raw_tx))
    {
        // error
        write_error_code(serial, error::bad_stream);
        outgoing_message response(request, result);
        queue_send(response);
        return;
    }

    auto ignore_send = [](const std::error_code&, network::channel_ptr node)
    {
    };

    // Send and hope for the best!
    node.protocol().broadcast(tx, ignore_send);

    // Response back to user saying everything is fine.
    write_error_code(serial, std::error_code());

    log_debug(LOG_SERVICE)
        << "protocol.broadcast_transaction() finished. Sending response.";

    outgoing_message response(request, result);
    queue_send(response);
}

void protocol_total_connections(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    BITCOIN_ASSERT(node.protocol().connection_count() <= max_uint32);
    const auto total_connections32 = static_cast<uint32_t>(
        node.protocol().connection_count());
    data_chunk result(8);
    auto serial = make_serializer(result.begin());
    write_error_code(serial, std::error_code());
    serial.write_4_bytes_little_endian(total_connections32);
    BITCOIN_ASSERT(serial.iterator() == result.end());

    log_debug(LOG_REQUEST)
        << "protocol.total_connections() finished. Sending response.";

    outgoing_message response(request, result);
    queue_send(response);
}

} // namespace server
} // namespace libbitcoin
