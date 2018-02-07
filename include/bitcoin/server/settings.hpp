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
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Common server configuration settings, properties not thread safe.
class BCS_API settings
{
public:
    settings();
    settings(system::config::settings context);

    /// Helpers.
    system::asio::duration heartbeat_interval() const;
    system::asio::duration subscription_expiration() const;
    const system::config::endpoint& zeromq_query_endpoint(bool secure) const;
    const system::config::endpoint& zeromq_heartbeat_endpoint(bool secure) const;
    const system::config::endpoint& zeromq_block_endpoint(bool secure) const;
    const system::config::endpoint& zeromq_transaction_endpoint(bool secure) const;

    const system::config::endpoint& websockets_query_endpoint(bool secure) const;
    const system::config::endpoint& websockets_heartbeat_endpoint(bool secure) const;
    const system::config::endpoint& websockets_block_endpoint(bool secure) const;
    const system::config::endpoint& websockets_transaction_endpoint(bool secure) const;

    /// [server]
    bool priority;
    bool secure_only;
    uint16_t query_workers;
    uint32_t subscription_limit;
    uint32_t subscription_expiration_minutes;
    uint32_t heartbeat_service_seconds;
    bool block_service_enabled;
    bool transaction_service_enabled;
    system::config::authority::list client_addresses;
    system::config::authority::list blacklists;

    /// [websockets]
    system::config::endpoint websockets_secure_query_endpoint;
    system::config::endpoint websockets_secure_heartbeat_endpoint;
    system::config::endpoint websockets_secure_block_endpoint;
    system::config::endpoint websockets_secure_transaction_endpoint;

    system::config::endpoint websockets_public_query_endpoint;
    system::config::endpoint websockets_public_heartbeat_endpoint;
    system::config::endpoint websockets_public_block_endpoint;
    system::config::endpoint websockets_public_transaction_endpoint;

    bool websockets_enabled;
    boost::filesystem::path websockets_root;
    boost::filesystem::path websockets_ca_certificate;
    boost::filesystem::path websockets_server_private_key;
    boost::filesystem::path websockets_server_certificate;
    boost::filesystem::path websockets_client_certificates;

    /// [zeromq]
    system::config::endpoint zeromq_secure_query_endpoint;
    system::config::endpoint zeromq_secure_heartbeat_endpoint;
    system::config::endpoint zeromq_secure_block_endpoint;
    system::config::endpoint zeromq_secure_transaction_endpoint;

    system::config::endpoint zeromq_public_query_endpoint;
    system::config::endpoint zeromq_public_heartbeat_endpoint;
    system::config::endpoint zeromq_public_block_endpoint;
    system::config::endpoint zeromq_public_transaction_endpoint;

    system::config::sodium zeromq_server_private_key;
    system::config::sodium::list zeromq_client_public_keys;
};

} // namespace server
} // namespace libbitcoin

#endif
