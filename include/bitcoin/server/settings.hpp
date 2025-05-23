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
#ifndef LIBBITCOIN_SERVER_SETTINGS_HPP
#define LIBBITCOIN_SERVER_SETTINGS_HPP

#include <chrono>
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
    settings(system::chain::selection context);

    /// Times.
    std::chrono::steady_clock::duration heartbeat_service() const;
    std::chrono::steady_clock::duration subscription_expiration() const;

    /// Endpoints.
    const protocol::endpoint& zeromq_query_endpoint(bool secure) const;
    const protocol::endpoint& zeromq_heartbeat_endpoint(bool secure) const;
    const protocol::endpoint& zeromq_block_endpoint(bool secure) const;
    const protocol::endpoint& zeromq_transaction_endpoint(bool secure) const;

    /// [server]
    bool priority;
    bool secure_only;
    uint16_t query_workers;
    uint32_t subscription_limit;
    uint32_t subscription_expiration_minutes;
    uint32_t heartbeat_service_seconds;
    bool block_service_enabled;
    bool transaction_service_enabled;
    protocol::authorities clients{};
    protocol::authorities blacklists{};

    /// [zeromq] secure
    protocol::endpoint zeromq_secure_query_endpoint;
    protocol::endpoint zeromq_secure_heartbeat_endpoint;
    protocol::endpoint zeromq_secure_block_endpoint;
    protocol::endpoint zeromq_secure_transaction_endpoint;

    /// [zeromq] clear
    protocol::endpoint zeromq_public_query_endpoint;
    protocol::endpoint zeromq_public_heartbeat_endpoint;
    protocol::endpoint zeromq_public_block_endpoint;
    protocol::endpoint zeromq_public_transaction_endpoint;

    /// [zeromq] keys
    protocol::sodium zeromq_server_private_key{};
    protocol::sodiums zeromq_client_public_keys{};
};

} // namespace server
} // namespace libbitcoin

#endif
