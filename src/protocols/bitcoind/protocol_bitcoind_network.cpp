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

namespace server {

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
    SUBSCRIBE_BITCOIND(handle_add_node, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_disconnect_node, _1, _2);
    SUBSCRIBE_BITCOIND(handle_export_asmap, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_added_node_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_addrman_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_connection_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_net_totals, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_node_addresses, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_peer_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_ping, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_network_active, _1, _2, _3);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Network methods.
// ----------------------------------------------------------------------------

// bitcoind's service name for each advertised service bit.
static array_t to_service_names(uint64_t services) NOEXCEPT
{
    using service = messages::peer::service;
    static const std::vector<std::pair<uint64_t, std::string>> names
    {
        { service::node_network, "NETWORK" },
        { service::node_bloom, "BLOOM" },
        { service::node_witness, "WITNESS" },
        { service::node_client_filters, "COMPACT_FILTERS" },
        { service::node_network_limited, "NETWORK_LIMITED" },
        { service::node_encrypted_transport, "P2P_V2" }
    };

    array_t out{};
    for (const auto& [bit, name]: names)
        if (to_bool(services & bit))
            out.emplace_back(name);

    return out;
}

bool protocol_bitcoind_network::handle_get_network_info(const code& ec,
    rpc_interface::get_network_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // bitcoind's numeric version encoding (10'000 major, 100 minor, patch).
    const auto& settings = server_settings().bitcoind;
    const auto& segments = settings.version.segments();
    const auto version = 10'000 * segments[0] + 100 * segments[1] + segments[2];

    // Proxied networks are not configurable (onion/i2p/cjdns unreachable).
    const auto network = [](const std::string& name) NOEXCEPT
    {
        return object_t
        {
            { "name", name },
            { "limited", false },
            { "reachable", true },
            { "proxy", std::string{} },
            { "proxy_randomize_credentials", false }
        };
    };

    array_t locals{};
    for (const auto& self: network_settings().inbound.selfs)
        locals.emplace_back(object_t
        {
            { "address", self.to_host() },
            { "port", self.port() },
            { "score", 1 }
        });

    const auto services = node_settings().services_provided();
    const auto connections = channel_count();
    const auto inbound = inbound_channel_count();

    send_result(object_t
    {
        { "version", version },
        { "subversion", settings.subversion },
        { "protocolversion", network_settings().protocol_maximum },
        { "localservices", encode_base16(to_big_endian(services)) },
        { "localservicesnames", to_service_names(services) },
        { "localrelay", network_settings().enable_relay },
        { "timeoffset", 0 },
        { "connections", connections },
        { "connections_in", inbound },
        { "connections_out", floored_subtract(connections, inbound) },
        { "networkactive", true },
        { "networks", array_t{ network("ipv4"), network("ipv6") } },
        { "relayfee", node_settings().minimum_fee_rate },
        { "incrementalfee", node_settings().minimum_bump_rate },
        { "localaddresses", std::move(locals) },
        { "warnings", std::string{} }
    }, 512);
    return true;
}

// The pool has no tried table, so all addresses are reported as new.
static object_t address_bucket(size_t count) NOEXCEPT
{
    return object_t
    {
        { "new", count },
        { "tried", zero },
        { "total", count }
    };
}

bool protocol_bitcoind_network::handle_get_addrman_info(const code& ec,
    rpc_interface::get_addrman_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    fetch_addresses(BIND(handle_fetch_info, _1, _2));
    return true;
}

void protocol_bitcoind_network::handle_fetch_info(const code& ec,
    const network::address_cptr& message) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_send_info, ec, message);
}

// An empty or unavailable pool is reported as empty (as bitcoind).
void protocol_bitcoind_network::do_send_info(const code& ec,
    const network::address_cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto empty = (ec || !message);

    size_t v4{};
    if (!empty)
        for (const auto& item: message->addresses)
            if (network::config::address{ item }.is_v4())
                ++v4;

    const auto total = empty ? zero : message->addresses.size();
    send_result(object_t
    {
        { "ipv4", address_bucket(v4) },
        { "ipv6", address_bucket(total - v4) },
        { "onion", address_bucket(zero) },
        { "i2p", address_bucket(zero) },
        { "cjdns", address_bucket(zero) },
        { "all_networks", address_bucket(total) }
    }, 512);
}

bool protocol_bitcoind_network::handle_get_node_addresses(const code& ec,
    rpc_interface::get_node_addresses, double count,
    const std::string& network) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!to_integer(node_count_, count) ||
        (!network.empty() && network != "ipv4" && network != "ipv6"))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    node_network_ = network;
    fetch_addresses(BIND(handle_fetch_nodes, _1, _2));
    return true;
}

void protocol_bitcoind_network::handle_fetch_nodes(const code& ec,
    const network::address_cptr& message) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_send_nodes, ec, message);
}

void protocol_bitcoind_network::do_send_nodes(const code& ec,
    const network::address_cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    // An empty or unavailable pool is reported as empty (as bitcoind).
    if (ec || !message)
    {
        send_result(array_t{}, 16);
        return;
    }

    // A zero count returns all addresses (as bitcoind).
    array_t out{};
    for (const auto& item: message->addresses)
    {
        if (!is_zero(node_count_) && out.size() >= node_count_)
            break;

        const network::config::address address{ item };
        const auto name = address.is_v4() ? "ipv4" : "ipv6";
        if (!node_network_.empty() && node_network_ != name)
            continue;

        out.emplace_back(object_t
        {
            { "time", item.timestamp },
            { "services", item.services },
            { "address", address.to_host() },
            { "port", item.port },
            { "network", std::string{ name } }
        });
    }

    const auto size = 128 * out.size();
    send_result(std::move(out), size);
}

// The nonce is discarded (pong correlation is a channel concern).
bool protocol_bitcoind_network::handle_ping(const code& ec,
    rpc_interface::ping) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_array<sizeof(uint64_t)> entropy{};
    pseudo_random::fill(entropy);
    const auto nonce = from_little_endian<uint64_t>(entropy);
    BROADCAST(peer::ping, to_shared<peer::ping>(nonce));
    send_result(null_t{}, 8);
    return true;
}

bool protocol_bitcoind_network::handle_set_network_active(const code& ec,
    rpc_interface::set_network_active, bool state) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (state)
        node::protocol::resume();
    else
        node::protocol::suspend(network::error::service_suspended);

    send_result(state, 8);
    return true;
}

bool protocol_bitcoind_network::handle_clear_banned(const code& ec,
    rpc_interface::clear_banned) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::database_error);
    return true;
}

bool protocol_bitcoind_network::handle_list_banned(const code& ec,
    rpc_interface::list_banned) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::database_error);
    return true;
}

bool protocol_bitcoind_network::handle_set_ban(const code& ec,
    rpc_interface::set_ban) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::database_error);
    return true;
}

// Removal requires manual session deregistration (not supported), and the
// transport is determined by the outbound privacy configuration.
bool protocol_bitcoind_network::handle_add_node(const code& ec,
    rpc_interface::add_node, const std::string& node,
    const std::string& command, bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (command != "add" && command != "onetry")
    {
        send_error(command == "remove" ? error::bitcoind::client_node_not_added :
            error::bitcoind::misc_error);
        return true;
    }

    // The endpoint parse throws on malformed input.
    try
    {
        connect(network::config::endpoint{ node });
    }
    catch (const std::exception&)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    send_result(null_t{}, 8);
    return true;
}

bool protocol_bitcoind_network::handle_disconnect_node(const code& ec,
    rpc_interface::disconnect_node) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_network::handle_export_asmap(const code& ec,
    rpc_interface::export_asmap) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_network::handle_get_added_node_info(const code& ec,
    rpc_interface::get_added_node_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_node_not_added);
    return true;
}

// Peer channels only (client channels are not connections).
bool protocol_bitcoind_network::handle_get_connection_count(const code& ec,
    rpc_interface::get_connection_count) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(channel_count(), 20);
    return true;
}

// Byte counters are not tracked (as the btcd endpoint reports). There is no
// upload target, which is the shape bitcoind reports for a disabled target.
bool protocol_bitcoind_network::handle_get_net_totals(const code& ec,
    rpc_interface::get_net_totals) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t target
    {
        { "timeframe", zero },
        { "target", zero },
        { "target_reached", false },
        { "serve_historical_blocks", true },
        { "bytes_left_in_cycle", zero },
        { "time_left_in_cycle", zero }
    };

    send_result(object_t
    {
        { "totalbytesrecv", zero },
        { "totalbytessent", zero },
        { "timemillis", possible_wide_cast<int64_t>(zulu_time()) * 1'000 },
        { "uploadtarget", std::move(target) }
    }, 256);
    return true;
}

bool protocol_bitcoind_network::handle_get_peer_info(const code& ec,
    rpc_interface::get_peer_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
