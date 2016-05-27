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
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define NAME "transaction_endpoint"

using std::placeholders::_1;
using namespace bc::chain;
using namespace bc::protocol;

transaction_endpoint::transaction_endpoint(zmq::authenticator& authenticator,
    server_node* node)
  : node_(node),
    socket_(authenticator, zmq::socket::role::pusher),
    settings_(node->server_settings())
{
    if (!settings_.transaction_endpoint_enabled)
        return;

    const auto secure = settings_.server_private_key;

    if (!authenticator.apply(socket_, NAME, secure))
        socket_.stop();
}

// The instance is retained in scope by subscribe_transactions until stopped.
bool transaction_endpoint::start()
{
    if (!settings_.transaction_endpoint_enabled)
    {
        stop();
        return true;
    }
    
    auto tx_endpoint = settings_.transaction_endpoint;

    if (!socket_ || !socket_.bind(tx_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize transaction publisher on " << tx_endpoint;

        stop();
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound transaction publish service on " << tx_endpoint;

    // This is not a libbitcoin re/subscriber.
    node_->subscribe_transactions(
        std::bind(&transaction_endpoint::send,
            this, _1));

    return true;
}

bool transaction_endpoint::stop()
{
    return socket_.stop();
}

void transaction_endpoint::send(const transaction& tx)
{
    zmq::message message;
    message.enqueue(tx.to_data());

    if (!message.send(socket_))
        log::warning(LOG_ENDPOINT)
            << "Failure publishing tx data.";
}

} // namespace server
} // namespace libbitcoin
