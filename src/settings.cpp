/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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

using namespace asio;

settings::settings()
  : priority(false),
    secure_only(false),
    query_workers(1),
    subscription_limit(1000),
    subscription_expiration_minutes(10),
    heartbeat_service_seconds(5),
    block_service_enabled(true),
    transaction_service_enabled(true),

    secure_query_endpoint("tcp://*:9081"),
    secure_heartbeat_endpoint("tcp://*:9082"),
    secure_block_endpoint("tcp://*:9083"),
    secure_transaction_endpoint("tcp://*:9084"),

    public_query_endpoint("tcp://*:9091"),
    public_heartbeat_endpoint("tcp://*:9092"),
    public_block_endpoint("tcp://*:9093"),
    public_transaction_endpoint("tcp://*:9094")
{
}

// There are no current distinctions spanning chain contexts.
settings::settings(bc::config::settings)
  : settings()
{
}

const config::endpoint& settings::query_endpoint(bool secure) const
{
    return secure ? secure_query_endpoint : public_query_endpoint;
}

const config::endpoint& settings::heartbeat_endpoint(bool secure) const
{
    return secure ? secure_heartbeat_endpoint : public_heartbeat_endpoint;
}

const config::endpoint& settings::block_endpoint(bool secure) const
{
    return secure ? secure_block_endpoint : public_block_endpoint;
}

const config::endpoint& settings::transaction_endpoint(bool secure) const
{
    return secure ? secure_transaction_endpoint : public_transaction_endpoint;
}

duration settings::heartbeat_interval() const
{
    return seconds(heartbeat_service_seconds);
}

duration settings::subscription_expiration() const
{
    return minutes(subscription_expiration_minutes);
}

} // namespace server
} // namespace libbitcoin
