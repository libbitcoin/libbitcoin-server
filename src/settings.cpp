/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/settings.hpp>

#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::system;
using namespace bc::system::asio;

settings::settings()
  : priority(false),
    secure_only(false),
    query_workers(1),
    subscription_limit(1000),
    subscription_expiration_minutes(10),
    heartbeat_service_seconds(5),
    block_service_enabled(true),
    transaction_service_enabled(true),

    // [websockets]
    websockets_secure_query_endpoint("tcp://*:9061"),
    websockets_secure_heartbeat_endpoint("tcp://*:9062"),
    websockets_secure_block_endpoint("tcp://*:9063"),
    websockets_secure_transaction_endpoint("tcp://*:9064"),

    websockets_public_query_endpoint("tcp://*:9071"),
    websockets_public_heartbeat_endpoint("tcp://*:9072"),
    websockets_public_block_endpoint("tcp://*:9073"),
    websockets_public_transaction_endpoint("tcp://*:9074"),

    websockets_enabled(true),
    websockets_root("web"),
    websockets_ca_certificate("ca.pem"),
    websockets_server_private_key("key.pem"),
    websockets_server_certificate("server.pem"),
    websockets_client_certificates("clients"),
    websockets_origins{},

    // [zeromq]
    zeromq_secure_query_endpoint("tcp://*:9081"),
    zeromq_secure_heartbeat_endpoint("tcp://*:9082"),
    zeromq_secure_block_endpoint("tcp://*:9083"),
    zeromq_secure_transaction_endpoint("tcp://*:9084"),

    zeromq_public_query_endpoint("tcp://*:9091"),
    zeromq_public_heartbeat_endpoint("tcp://*:9092"),
    zeromq_public_block_endpoint("tcp://*:9093"),
    zeromq_public_transaction_endpoint("tcp://*:9094")
{
}

// There are no current distinctions spanning chain contexts.
settings::settings(config::settings)
  : settings()
{
}

duration settings::heartbeat_interval() const
{
    return seconds(heartbeat_service_seconds);
}

duration settings::subscription_expiration() const
{
    return minutes(subscription_expiration_minutes);
}

const config::endpoint& settings::websockets_query_endpoint(bool secure) const
{
    return secure ? websockets_secure_query_endpoint :
        websockets_public_query_endpoint;
}

const config::endpoint& settings::websockets_heartbeat_endpoint(bool secure) const
{
    return secure ? websockets_secure_heartbeat_endpoint :
        websockets_public_heartbeat_endpoint;
}

const config::endpoint& settings::websockets_block_endpoint(bool secure) const
{
    return secure ? websockets_secure_block_endpoint :
        websockets_public_block_endpoint;
}

const config::endpoint& settings::websockets_transaction_endpoint(
    bool secure) const
{
    return secure ? websockets_secure_transaction_endpoint :
        websockets_public_transaction_endpoint;
}

const config::endpoint& settings::zeromq_query_endpoint(bool secure) const
{
    return secure ? zeromq_secure_query_endpoint :
        zeromq_public_query_endpoint;
}

const config::endpoint& settings::zeromq_heartbeat_endpoint(bool secure) const
{
    return secure ? zeromq_secure_heartbeat_endpoint :
        zeromq_public_heartbeat_endpoint;
}

const config::endpoint& settings::zeromq_block_endpoint(bool secure) const
{
    return secure ? zeromq_secure_block_endpoint :
        zeromq_public_block_endpoint;
}

const config::endpoint& settings::zeromq_transaction_endpoint(bool secure) const
{
    return secure ? zeromq_secure_transaction_endpoint :
        zeromq_public_transaction_endpoint;
}

} // namespace server
} // namespace libbitcoin
