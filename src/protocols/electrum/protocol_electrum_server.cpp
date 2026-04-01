/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <map>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_electrum::handle_server_add_peer(const code& ec,
    rpc_interface::server_add_peer, const interface::object_t& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_banner(const code& ec,
    rpc_interface::server_banner) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(options().banner_message, 42, BIND(complete, _1));
}

void protocol_electrum::handle_server_donation_address(const code& ec,
    rpc_interface::server_donation_address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(options().donation_address, 42, BIND(complete, _1));
}

void protocol_electrum::handle_server_features(const code& ec,
    rpc_interface::server_features) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    const auto& query = archive();
    const auto genesis = query.to_confirmed(zero);
    if (genesis.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    const auto hash = query.get_header_key(genesis);
    if (hash == null_hash)
    {
        send_code(error::server_error);
        return;
    }

    send_result(object_t
    {
        { "genesis_hash", encode_hash(hash) },
        { "hosts", advertised_hosts() },
        { "hash_function", "sha256" },
        { "server_version", options().server_name },
        { "protocol_min", string_t{ version_to_string(minimum) } },
        { "protocol_max", string_t{ version_to_string(maximum) } },
        { "pruning", null_t{} }
    }, 1024, BIND(complete, _1));
}

// This is not actually a subscription method.
void protocol_electrum::handle_server_peers_subscribe(const code& ec,
    rpc_interface::server_peers_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_ping(const code& ec,
    rpc_interface::server_ping) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::wrong_version);
        return;
    }

    // Any receive, including ping, resets the base channel inactivity timer.
    send_result(null_t{}, 42, BIND(complete, _1));
}

// utilities
// ----------------------------------------------------------------------------

// One of each type allowed for given host, last writer wins if more than one.
object_t protocol_electrum::advertised_hosts() const NOEXCEPT
{
    std::map<string_t, object_t> map{};

    for (const auto& bind: options().advertise_binds)
        if (!bind.host().empty())
            map[bind.host()]["tcp_port"] = bind.port();

    for (const auto& safe: options().advertise_safes)
        if (!safe.host().empty())
            map[safe.host()]["ssl_port"] = safe.port();

    object_t hosts{};
    for (const auto& [host, object] : map)
        hosts[host] = object;

    if (hosts.empty()) return
    {
        { "tcp_port", null_t{} },
        { "ssl_port", null_t{} }
    };

    return hosts;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
