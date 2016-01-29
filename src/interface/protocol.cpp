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
#include <bitcoin/server.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include "utility.hpp"

namespace libbitcoin {
namespace server {

void protocol::broadcast_transaction(server_node& node,
    const incoming_message& request, send_handler handler)
{
    const data_chunk& raw_tx = request.data();
    chain::transaction tx;
    data_chunk result(code_size);
    auto serial = make_serializer(result.begin());

    if (!tx.from_data(raw_tx))
    {
        // error
        serial.write_error_code(error::bad_stream);
        outgoing_message response(request, result);
        handler(response);
        return;
    }

    const auto ignore_complete = [](const code&) {};
    const auto ignore_send = [](const code&, network::channel::ptr) {};

    // Send and hope for the best!
    node.broadcast(tx, ignore_send, ignore_complete);

    // Response back to user saying everything is fine.
    serial.write_error_code(error::success);

    log::debug(LOG_SERVICE)
        << "protocol.broadcast_transaction() finished. Sending response.";

    outgoing_message response(request, result);
    handler(response);
}

void protocol::total_connections(server_node& node,
    const incoming_message& request, send_handler handler)
{
    // TODO:
    ////node.connected_count(handler);
    const uint32_t total_connections = 0;

    data_chunk result(code_size + sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    serial.write_error_code(error::success);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);
    serial.write_4_bytes_little_endian(total_connections);
    BITCOIN_ASSERT(serial.iterator() == result.end());

    log::debug(LOG_REQUEST)
        << "protocol.total_connections() finished. Sending response.";

    outgoing_message response(request, result);
    handler(response);
}

} // namespace server
} // namespace libbitcoin
