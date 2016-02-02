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

using namespace config;

static const settings mainnet_defaults()
{
    settings value;
    value.threads = 2;
    value.heartbeat_interval_seconds = 5;
    value.polling_interval_milliseconds = 1;
    value.subscription_expiration_minutes = 10;
    value.subscription_limit = 100000000;
    value.publisher_enabled = true;
    value.queries_enabled = true;
    value.log_requests = false;
    value.query_endpoint = { "tcp://*:9091" };
    value.heartbeat_endpoint = { "tcp://*:9092" };
    value.block_publish_endpoint = { "tcp://*:9093" };
    value.transaction_publish_endpoint = { "tcp://*:9094" };
    value.certificate_file = {};
    value.client_certificates_path = {};
    value.whitelists = {};
    return value;
};

static const settings testnet_defaults()
{
    return mainnet_defaults();
};

const settings settings::mainnet = mainnet_defaults();
const settings settings::testnet = testnet_defaults();

asio::duration settings::polling_interval() const
{
    return asio::milliseconds(polling_interval_milliseconds);
}

asio::duration settings::heartbeat_interval() const
{
    return asio::seconds(polling_interval_milliseconds);
}

asio::duration settings::subscription_expiration() const
{
    return asio::minutes(subscription_expiration_minutes);
}

} // namespace server
} // namespace libbitcoin
