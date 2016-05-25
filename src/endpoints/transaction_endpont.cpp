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
#include <bitcoin/server/endpoints/transaction_endpoint.hpp>

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using namespace bc::chain;
using namespace bc::protocol;

transaction_endpoint::transaction_endpoint(zmq::context::ptr context,
    server_node* node)
  : node_(node),
    socket_(*context, zmq::socket::role::pusher),
    settings_(node->server_settings())
{
    BITCOIN_ASSERT(node_);
    BITCOIN_ASSERT(socket_);
}

// The instance is retained in scope by subscribe_transactions until stopped.
bool transaction_endpoint::start()
{
    if (!settings_.transaction_endpoint_enabled)
        return true;
    
    auto tx_endpoint = settings_.transaction_endpoint.to_string();

    if (!socket_.set_authentication_domain("transaction") ||
        !socket_.bind(tx_endpoint))
    {
        log::error(LOG_SERVICE)
            << "Failed to start transaction publisher on " << tx_endpoint;
        return false;
    }

    log::info(LOG_SERVICE)
        << "Bound transaction publish service on " << tx_endpoint;

    // This is not a libbitcoin re/subscriber.
    node_->subscribe_transactions(
        std::bind(&transaction_endpoint::send,
            shared_from_this(), _1));

    return true;
}

void transaction_endpoint::send(const transaction& tx)
{
    zmq::message message;
    message.enqueue(tx.to_data());

    if (!message.send(socket_))
        log::warning(LOG_SERVICE)
            << "Failure publishing tx data.";
}

} // namespace server
} // namespace libbitcoin
