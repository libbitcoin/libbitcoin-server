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
#include <bitcoin/server/protocols/protocol_btcd_network.hpp>

#include <chrono>
#include <format>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_btcd_network::send_absent(const code&) NOEXCEPT
{
    send_error_message(error::btcd::invalid_parameter, "peer not found");
}

static double to_microseconds(
    const network::steady_clock::duration& span) NOEXCEPT
{
    using namespace std::chrono;
    return system::to_floating(duration_cast<microseconds>(span).count());
}

// btcd reports services in decimal, ping times in microseconds, and its own
// relay state in place of bitcoind's connection detail.
void protocol_btcd_network::finish_peer(object_t& out,
    const network::diagnostics::row& row) const NOEXCEPT
{
    static const std::array<std::string, 14> bitcoind_keys
    {
        "addrbind", "network", "servicesnames", "minfeefilter",
        "connection_type", "presynced_headers", "synced_headers",
        "synced_blocks", "inflight", "permissions", "bytessent_per_msg",
        "bytesrecv_per_msg", "transport_protocol_type", "minping"
    };

    for (const auto& key: bitcoind_keys)
        out.erase(key);

    out["services"] = std::format("{:08}", row.peer_services);
    out["pingtime"] = to_microseconds(row.ping_time);
    if (out.contains("pingwait"))
        out["pingwait"] = to_microseconds(row.pending_ping_time);

    out["banscore"] = 0;
    out["feefilter"] = row.peer_minimum_fee;
    out["syncnode"] = false;
    out["v2_connection"] = row.encrypted;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
