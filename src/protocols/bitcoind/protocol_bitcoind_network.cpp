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

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

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
    SUBSCRIBE_BITCOIND(handle_export_asmap, _1, _2, _3);
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
        if (to_bool(bit_and(services, bit)))
            out.emplace_back(name);

    return out;
}

bool protocol_bitcoind_network::handle_get_network_info(const code& ec,
    rpc_interface::get_network_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // bitcoind's numeric version encoding (10'000 major, 100 minor, patch).
    const auto& settings = options();
    const auto& segments = settings.version.segments();
    const auto version = 10'000 * segments[0] + 100 * segments[1] + segments[2];

    // Proxy credentials are configured, so are never randomized.
    const auto network = [](const std::string& name, bool reachable,
        const std::string& proxy) NOEXCEPT
    {
        return object_t
        {
            { "name", name },
            { "limited", !reachable },
            { "reachable", reachable },
            { "proxy", proxy },
            { "proxy_randomize_credentials", false }
        };
    };

    const auto local = [](const network::config::address& self) NOEXCEPT
    {
        return object_t
        {
            { "address", self.to_host() },
            { "port", self.port() },
            { "score", 1 }
        };
    };

    const auto& net_settings = network_settings();
    const auto proxied = net_settings.outbound.proxied();
    const auto bridged = net_settings.inbound.bridged();
    const auto proxy = proxied ? net_settings.outbound.socks.to_string() :
        std::string{};
    const auto bridge = bridged ? net_settings.inbound.bridge.to_string() :
        proxy;

    array_t locals{};
    for (const auto& self: net_settings.inbound.selfs)
        locals.emplace_back(local(self));

    if (const auto& sam = net_settings.inbound.self)
        locals.emplace_back(local(sam));

    const auto services = node_settings().services_provided();
    const auto connections = channel_count();
    const auto inbound = inbound_channel_count();

    send_result(object_t
    {
        { "version", version },
        { "subversion", settings.subversion },
        { "protocolversion", net_settings.protocol_maximum },
        { "localservices", encode_base16(to_big_endian(services)) },
        { "localservicesnames", to_service_names(services) },
        { "localrelay", net_settings.enable_relay },
        { "timeoffset", 0 },
        { "connections", connections },
        { "connections_in", inbound },
        { "connections_out", floored_subtract(connections, inbound) },
        { "networkactive", !node::protocol::suspended() },
        { "networks", array_t
        {
            network("ipv4", net_settings.gossip_ipv4, proxy),
            network("ipv6", net_settings.gossip_ipv6, proxy),
            network("onion", net_settings.gossip_tor && proxied, proxy),
            network("i2p", net_settings.gossip_i2p && (proxied || bridged), bridge),
            network("cjdns", net_settings.gossip_ipv6, proxy)
        } },
        { "relayfee", node_settings().minimum_fee_rate },
        { "incrementalfee", node_settings().minimum_bump_rate },
        { "localaddresses", std::move(locals) },
        { "warnings", array_t{} }
    }, 1024);
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
// transport is determined by the outbound p2ps configuration.
bool protocol_bitcoind_network::handle_add_node(const code& ec,
    rpc_interface::add_node, const std::string& node,
    const std::string& command, bool v2transport) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // bitcoind reports v2transport as invalid when not enabled.
    if (v2transport)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

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
    rpc_interface::export_asmap, const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::misc_error);
    return true;
}

bool protocol_bitcoind_network::handle_get_added_node_info(const code& ec,
    rpc_interface::get_added_node_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_node_not_added);
    return true;
}

// bitcoind's network name for each address type, indexed by network id.
// Both tor versions are reported as onion, the unspecified network is unnamed.
constexpr std::array<std::string_view, network::config::address_types>
network_names
{
    "", "ipv4", "ipv6", "onion", "onion", "i2p", "cjdns"
};

static std::string to_network_name(
    const network::config::address& address) NOEXCEPT
{
    const network::messages::peer::address_item& item = address;
    return std::string{ network_names.at(item.address.index()) };
}

// The pool has no tried table (by design), so all addresses report as new.
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

    using namespace network::messages::peer;
    const auto counts = address_counts();
    const auto ipv4 = counts.at(ipv4_t::id);
    const auto ipv6 = counts.at(ipv6_t::id);
    const auto onion = counts.at(torv3_t::id);
    const auto i2p = counts.at(i2p_t::id);
    const auto cjdns = counts.at(cjdns_t::id);

    send_result(object_t
    {
        { "ipv4", address_bucket(ipv4) },
        { "ipv6", address_bucket(ipv6) },
        { "onion", address_bucket(onion) },
        { "i2p", address_bucket(i2p) },
        { "cjdns", address_bucket(cjdns) },
        { "all_networks", address_bucket(ipv4 + ipv6 + onion + i2p + cjdns) }
    }, 512);
    return true;
}

bool protocol_bitcoind_network::handle_get_node_addresses(const code& ec,
    rpc_interface::get_node_addresses, double count,
    const std::string& network) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!to_integer(node_count_, count) ||
        (!network.empty() && !contains(network_names, network)))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    node_network_ = network;
    dump_addresses(BIND(handle_dump_nodes, _1, _2));
    return true;
}

void protocol_bitcoind_network::handle_dump_nodes(const code& ec,
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

    // An empty or unavailable pool is reported as empty.
    if (ec || !message)
    {
        send_result(array_t{}, 16);
        return;
    }

    // A zero count does not limit the dump (a randomized pool subset).
    array_t out{};
    for (const auto& item: message->addresses)
    {
        if (!is_zero(node_count_) && out.size() >= node_count_)
            break;

        const network::config::address address{ item };
        const auto name = network_names.at(item.address.index());
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

// An injected ping would violate channel pong correlation (no-op).
bool protocol_bitcoind_network::handle_ping(const code& ec,
    rpc_interface::ping) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(null_t{}, 8);
    return true;
}

bool protocol_bitcoind_network::handle_set_network_active(const code& ec,
    rpc_interface::set_network_active, bool state) NOEXCEPT
{
    if (stopped(ec))
        return false;

    auto active = false;
    if (state)
    {
        // Resume is refused on a full or faulted store.
        active = node::protocol::resume();
    }
    else
    {
        node::protocol::suspend(network::error::service_suspended);
    }

    send_result(active, 8);
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

bool protocol_bitcoind_network::handle_get_net_totals(const code& ec,
    rpc_interface::get_net_totals) NOEXCEPT
{
    if (stopped(ec))
        return false;

    capture_totals(BIND(do_send_net_totals, _1, _2, _3));
    return true;
}

void protocol_bitcoind_network::do_send_net_totals(const code& ec,
    uint64_t sent, uint64_t received) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        send_error(error::bitcoind::misc_error);
        return;
    }

    // Outbound is rate limited, so the target is the most that automatic
    // connections can send in the timeframe. Manual connections are added by
    // the operator and are not bounded by configuration. An unlimited rate
    // on a connectable section is reported as no target (as bitcoind).
    constexpr uint64_t timeframe = 24 * 60 * 60;
    const auto& net_settings = network_settings();
    const auto& in = net_settings.inbound;
    const auto& out = net_settings.outbound;
    const auto in_rate = net_settings.rate_limited(in);
    const auto out_rate = net_settings.rate_limited(out);

    const auto unlimited =
        (to_bool(in.connections) && is_zero(in_rate)) ||
        (to_bool(out.connections) && is_zero(out_rate));

    const auto limit = ceilinged_add(
        ceilinged_multiply<uint64_t>(in.connections, in_rate),
        ceilinged_multiply<uint64_t>(out.connections, out_rate));

    const auto bytes = unlimited ? zero :
        ceilinged_multiply<uint64_t>(limit, timeframe);

    // The rate bound does not deplete, so a full cycle always remains.
    object_t target
    {
        { "timeframe", timeframe },
        { "target", bytes },
        { "target_reached", false },
        { "serve_historical_blocks", true },
        { "bytes_left_in_cycle", bytes },
        { "time_left_in_cycle", timeframe }
    };

    send_result(object_t
    {
        { "totalbytesrecv", received },
        { "totalbytessent", sent },
        { "timemillis", possible_wide_cast<int64_t>(zulu_time()) * 1'000 },
        { "uploadtarget", std::move(target) }
    }, 256);
}

// bitcoind's connection type name for each capture group.
static std::string to_connection_type(
    network::diagnostics::target group) NOEXCEPT
{
    using target = network::diagnostics::target;
    switch (group)
    {
        case target::inbound: return "inbound";
        case target::manual: return "manual";
        default: return "outbound-full-relay";
    }
}

// The round completes when the last captured channel releases the message.
bool protocol_bitcoind_network::handle_get_peer_info(const code& ec,
    rpc_interface::get_peer_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto captured = std::make_shared<network::diagnostics::sink>();
    const auto complete = std::make_shared<network::diagnostics::race>(
        BIND(handle_captured, _1, captured));

    BROADCAST(network::diagnostics, to_shared<const network::diagnostics>(
        complete, captured, network::diagnostics::target::all));

    return true;
}

void protocol_bitcoind_network::handle_captured(const code&,
    const network::diagnostics::sink::ptr& captured) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_send_peer_info, captured);
}

void protocol_bitcoind_network::do_send_peer_info(
    const network::diagnostics::sink::ptr& captured) NOEXCEPT
{
    BC_ASSERT(stranded());

    array_t out{};
    for (const auto& row: captured->captured())
    {
        object_t info
        {
            { "id", row.identifier },
            { "addr", network::config::endpoint{ row.address }.to_string() },
            { "addrbind", row.binding.to_string() },
            { "network", to_network_name(row.address) },
            { "services", encode_base16(to_big_endian(row.services)) },
            { "servicesnames", to_service_names(row.services) },
            { "relaytxes", row.relay },
            { "connection_type", to_connection_type(row.group) },
            { "inbound", row.group == network::diagnostics::target::inbound },
            { "version", row.version },
            { "subver", row.agent },
            { "startingheight", row.start_height },
            { "conntime", row.created },
            { "timeoffset", row.time_offset },
            { "lastsend", row.last_write },
            { "lastrecv", row.last_read },
            { "bytessent", row.sent },
            { "bytesrecv", row.received },
            { "transport_protocol_type", row.encrypted ? "v2" : "v1" }
        };

        // The local address is unknown unless the peer has provided it.
        if (row.local)
            info.emplace("addrlocal",
                network::config::endpoint{ row.local }.to_string());

        out.emplace_back(std::move(info));
    }

    const auto size = 256 * out.size();
    send_result(std::move(out), size);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
