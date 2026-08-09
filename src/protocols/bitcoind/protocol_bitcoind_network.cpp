/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/server/protocols/protocol_bitcoind_network.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

// Isolate the subgroup dispatch metaprogramming to this translation unit.
template class network::rpc::dispatcher<
    server::interface::bitcoind_network>;

namespace server {

template class protocol_bitcoind_dispatch<interface::bitcoind_network>;

#define CLASS protocol_bitcoind_network
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_network::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_network_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_clear_banned, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_banned, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_ban, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_node, _1, _2);
    SUBSCRIBE_BITCOIND(handle_disconnect_node, _1, _2);
    SUBSCRIBE_BITCOIND(handle_export_asmap, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_added_node_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_addrman_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_connection_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_net_totals, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_node_addresses, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_peer_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_ping, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_network_active, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Network methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_network::handle_get_network_info(const code& ec,
    rpc_interface::get_network_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // bitcoind's numeric version encoding (10'000 major, 100 minor, patch).
    const auto& settings = server_settings().bitcoind;
    const auto& segments = settings.version.segments();
    const auto version = 10'000 * segments[0] + 100 * segments[1] + segments[2];

    send_result(object_t
    {
        { "version", version },
        { "subversion", settings.subversion },
        { "protocolversion", network_settings().protocol_maximum },
        { "localrelay", true },
        { "timeoffset", 0 },
        { "connections", 0 },
        { "networkactive", true },
        { "networks", array_t{} },
        { "relayfee", node_settings().minimum_fee_rate },
        { "incrementalfee", node_settings().minimum_bump_rate },
        { "localaddresses", array_t{} },
        { "warnings", std::string{} }
    }, 256);
    return true;
}

bool protocol_bitcoind_network::handle_clear_banned(const code& ec,
    rpc_interface::clear_banned) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_list_banned(const code& ec,
    rpc_interface::list_banned) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_set_ban(const code& ec,
    rpc_interface::set_ban) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_add_node(const code& ec,
    rpc_interface::add_node) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_disconnect_node(const code& ec,
    rpc_interface::disconnect_node) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_export_asmap(const code& ec,
    rpc_interface::export_asmap) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_get_added_node_info(const code& ec,
    rpc_interface::get_added_node_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_get_addrman_info(const code& ec,
    rpc_interface::get_addrman_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_get_connection_count(const code& ec,
    rpc_interface::get_connection_count) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_get_net_totals(const code& ec,
    rpc_interface::get_net_totals) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_get_node_addresses(const code& ec,
    rpc_interface::get_node_addresses) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_get_peer_info(const code& ec,
    rpc_interface::get_peer_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_ping(const code& ec,
    rpc_interface::ping) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_network::handle_set_network_active(const code& ec,
    rpc_interface::set_network_active) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
