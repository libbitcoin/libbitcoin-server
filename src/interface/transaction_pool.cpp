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
#include <bitcoin/server/interface/transaction_pool.hpp>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::message;

void transaction_pool::fetch_transaction(server_node& node,
    const message& request, send_handler handler)
{
    hash_digest hash;

    if (!unwrap_fetch_transaction_args(hash, request))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    LOG_DEBUG(LOG_SERVER)
        << "transaction_pool.fetch_transaction(" << encode_hash(hash) << ")";

    // TODO: implement query on blockchain interface.
    ////////////node.chain().fetch_pool_transaction(hash,
    ////////////    std::bind(pool_transaction_fetched,
    ////////////        _1, _2, request, handler));
}

// Broadcast a transaction with penetration subscription.
void transaction_pool::broadcast(server_node& node, const message& request,
    send_handler handler)
{
    transaction tx;

    if (!tx.from_data(request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // TODO: conditionally subscribe to penetration notifications.
    // TODO: broadcast transaction to receiving peers.
    handler(message(request, error::operation_failed));
}

// NOTE: the format of this response changed in v3 (send only code on error).
void transaction_pool::validate(server_node& node, const message& request,
    send_handler handler)
{
    static const auto version = bc::message::version::level::maximum;
    const auto tx = std::make_shared<transaction_message>();

    if (!tx->from_data(version, request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // TODO: implement query on blockchain interface.
    //////////node.chain().validate(tx,
    //////////    std::bind(&transaction_pool::handle_validated,
    //////////        _1, _2, request, handler));
}

void transaction_pool::handle_validated(const code& ec,
    const point::indexes& unconfirmed, const message& request,
    send_handler handler)
{
    // [ code:4 ]
    // [[ unconfirmed_index:4 ]...]
    data_chunk result(code_size + unconfirmed.size() * index_size);
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);

    for (const auto unconfirmed_index: unconfirmed)
        serial.write_4_bytes_little_endian(unconfirmed_index);

    handler(message(request, result));
}

} // namespace server
} // namespace libbitcoin
