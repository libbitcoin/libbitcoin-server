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

// BUGBUG: reconnect to tx pool when complete.
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
    handler(message(request, error::not_implemented));
}

// BUGBUG: reconnect to tx pool when complete.
// Broadcast a transaction with penetration subscription.
void transaction_pool::broadcast(server_node& node, const message& request,
    send_handler handler)
{
    chain::transaction tx;

    if (!tx.from_data(request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // TODO: broadcast transaction to receiving peers.
    // TODO: conditionally subscribe to penetration notifications.
    handler(message(request, error::not_implemented));
}

// BUGBUG: reconnect to tx pool when complete.
void transaction_pool::validate2(server_node& node, const message& request,
    send_handler handler)
{
    static const auto version = bc::message::version::level::maximum;
    const auto tx = std::make_shared<bc::message::transaction>();

    if (!tx->from_data(version, request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // TODO: implement query on blockchain interface.
    //////////node.chain().validate(tx,
    //////////    std::bind(&transaction_pool::handle_validated,
    //////////        _1, request, handler));
    handler(message(request, error::not_implemented));
}

// TODO: update client and explorer.
void transaction_pool::handle_validated2(const code& ec,
    const message& request, send_handler handler)
{
    // Returns validation error or error::success.
    handler(message(request, ec));
}

} // namespace server
} // namespace libbitcoin
