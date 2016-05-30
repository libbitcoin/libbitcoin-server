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
#include <bitcoin/server/endpoints/trans_endpoint.hpp>

#include <cstdint>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define PUBLIC_NAME "public_transaction"
#define SECURE_NAME "secure_transaction"

using std::placeholders::_1;
using namespace bc::chain;
using namespace bc::protocol;

static inline bool is_enabled(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return settings.transaction_endpoints_enabled &&
        (!secure || settings.server_private_key);
}

static inline config::endpoint get_endpoint(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return secure ? settings.secure_transaction_endpoint :
        settings.public_transaction_endpoint;
}

trans_endpoint::trans_endpoint(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : node_(node),
    socket_(authenticator, zmq::socket::role::pusher),
    endpoint_(get_endpoint(node, secure)),
    enabled_(is_enabled(node, secure)),
    secure_(secure)
{
    const auto name = secure ? SECURE_NAME : PUBLIC_NAME;

    // The authenticator logs apply failures and stopped socket halts start.
    if (!enabled_ || !authenticator.apply(socket_, name, secure))
        socket_.stop();
}

// The endpoint is not restartable.
// The instance is retained in scope by subscribe_transactions until stopped.
bool trans_endpoint::start()
{
    if (!enabled_)
        return true;

    if (!socket_)
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize transaction publisher.";
        return false;
    }

    if (!socket_.bind(endpoint_))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to bind transaction publish to " << endpoint_;
        stop();
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound " << (secure_ ? "secure " : "public ")
        << "transaction publish service to " << endpoint_;

    // This is not a libbitcoin re/subscriber.
    node_.subscribe_transactions(
        std::bind(&trans_endpoint::send,
            this, _1));

    return true;
}

bool trans_endpoint::stop()
{
    if (socket_)
        log::debug(LOG_ENDPOINT)
            << "Unbound " << (secure_ ? "secure " : "public ")
            << "transaction publish service to " << endpoint_;

    return socket_.stop();
}

void trans_endpoint::send(const transaction& tx)
{
    zmq::message message;
    message.enqueue(tx.to_data());

    if (!message.send(socket_))
        log::warning(LOG_ENDPOINT)
            << "Failure publishing tx data.";
}

} // namespace server
} // namespace libbitcoin
