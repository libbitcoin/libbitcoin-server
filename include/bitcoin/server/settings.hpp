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
#ifndef LIBBITCOIN_SERVER_SETTINGS_HPP
#define LIBBITCOIN_SERVER_SETTINGS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Common database configuration settings, properties not thread safe.
class BCS_API settings
{
public:
    settings();
    settings(system::config::settings context);

    /// Helpers.
    system::asio::duration heartbeat_interval() const;
    system::asio::duration subscription_expiration() const;
    const system::config::endpoint& query_endpoint(bool secure) const;
    const system::config::endpoint& heartbeat_endpoint(bool secure) const;
    const system::config::endpoint& block_endpoint(bool secure) const;
    const system::config::endpoint& transaction_endpoint(bool secure) const;

    /// Properties.
    bool priority;
    bool secure_only;

    uint16_t query_workers;
    uint32_t subscription_limit;
    uint32_t subscription_expiration_minutes;
    uint32_t heartbeat_service_seconds;
    bool block_service_enabled;
    bool transaction_service_enabled;

    system::config::endpoint secure_query_endpoint;
    system::config::endpoint secure_heartbeat_endpoint;
    system::config::endpoint secure_block_endpoint;
    system::config::endpoint secure_transaction_endpoint;

    system::config::endpoint public_query_endpoint;
    system::config::endpoint public_heartbeat_endpoint;
    system::config::endpoint public_block_endpoint;
    system::config::endpoint public_transaction_endpoint;

    system::config::sodium server_private_key;
    system::config::sodium::list client_public_keys;
    system::config::authority::list client_addresses;
    system::config::authority::list blacklists;
};

} // namespace server
} // namespace libbitcoin

#endif
