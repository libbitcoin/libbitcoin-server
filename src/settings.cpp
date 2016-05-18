/**
 * Copyright (c) 2011-2016 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/settings.hpp>

#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace server {

using namespace asio;

settings::settings()
  : threads(2),
    heartbeat_interval_seconds(5),
    polling_interval_microseconds(1),
    subscription_expiration_minutes(10),
    subscription_limit(100000000),
    publisher_enabled(true),
    queries_enabled(true),
    log_requests(false),
    query_endpoint("tcp://*:9091"),
    heartbeat_endpoint("tcp://*:9092"),
    block_publish_endpoint("tcp://*:9093"),
    transaction_publish_endpoint("tcp://*:9094")
{
}

// There are no current distinctions spanning chain contexts.
settings::settings(bc::settings context)
  : settings()
{
}

duration settings::polling_interval() const
{
    return microseconds(polling_interval_microseconds);
}

duration settings::heartbeat_interval() const
{
    return seconds(heartbeat_interval_seconds);
}

duration settings::subscription_expiration() const
{
    return minutes(subscription_expiration_minutes);
}

} // namespace server
} // namespace libbitcoin
