/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

#include <chrono>
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace protocol;
using namespace std::chrono;

settings::settings()
  : // [server]
    priority(false),
    secure_only(false),
    query_workers(1),
    subscription_limit(1000),
    subscription_expiration_minutes(10),
    heartbeat_service_seconds(5),
    block_service_enabled(true),
    transaction_service_enabled(true),

    // [zeromq] secure
    zeromq_secure_query_endpoint("tcp://*:9081"),
    zeromq_secure_heartbeat_endpoint("tcp://*:9082"),
    zeromq_secure_block_endpoint("tcp://*:9083"),
    zeromq_secure_transaction_endpoint("tcp://*:9084"),

    // [zeromq] clear
    zeromq_public_query_endpoint("tcp://*:9091"),
    zeromq_public_heartbeat_endpoint("tcp://*:9092"),
    zeromq_public_block_endpoint("tcp://*:9093"),
    zeromq_public_transaction_endpoint("tcp://*:9094")
{
}

settings::settings(chain::selection context)
  : settings()
{
}

steady_clock::duration settings::heartbeat_service() const
{
    return seconds(heartbeat_service_seconds);
}

steady_clock::duration settings::subscription_expiration() const
{
    return minutes(subscription_expiration_minutes);
}

const endpoint& settings::zeromq_query_endpoint(bool secure) const
{
    return secure ? zeromq_secure_query_endpoint :
        zeromq_public_query_endpoint;
}

const endpoint& settings::zeromq_heartbeat_endpoint(bool secure) const
{
    return secure ? zeromq_secure_heartbeat_endpoint :
        zeromq_public_heartbeat_endpoint;
}

const endpoint& settings::zeromq_block_endpoint(bool secure) const
{
    return secure ? zeromq_secure_block_endpoint :
        zeromq_public_block_endpoint;
}

const endpoint& settings::zeromq_transaction_endpoint(bool secure) const
{
    return secure ? zeromq_secure_transaction_endpoint :
        zeromq_public_transaction_endpoint;
}

} // namespace server
} // namespace libbitcoin
