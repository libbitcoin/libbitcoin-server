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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_NETWORK_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_NETWORK_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_network_methods
{
    static constexpr std::tuple methods
    {
        method<"getnetworkinfo">{},
        method<"clearbanned">{ unimplemented },
        method<"listbanned">{ unimplemented },
        method<"setban", string_t, string_t, nullopt<0.0>, nullopt<false>>{ unimplemented, "subnet", "command", "bantime", "absolute" },
        method<"addnode", string_t, string_t, nullopt<false>, command>{ "node", "command", "v2transport" },
        method<"disconnectnode", nullable<string_t>, nullable<number_t>, command>{ "address", "nodeid" },
        method<"exportasmap", string_t>{ "path" },
        method<"getaddednodeinfo", nullable<string_t>>{ "node" },
        method<"getaddrmaninfo">{},
        method<"getconnectioncount">{},
        method<"getnettotals">{},
        method<"getnodeaddresses", nullopt<1.0>, nullopt<""_t>>{ "count", "network" },
        method<"getpeerinfo">{},
        method<"ping">{},
        method<"setnetworkactive", boolean_t, command>{ "state" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    /// Method names as reported by help.
    static constexpr auto name_data = method_names<methods>();
    static constexpr std::string_view names{ name_data.data(),
        name_data.size() };

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using get_network_info = at<0>;
    using clear_banned = at<1>;
    using list_banned = at<2>;
    using set_ban = at<3>;
    using add_node = at<4>;
    using disconnect_node = at<5>;
    using export_asmap = at<6>;
    using get_added_node_info = at<7>;
    using get_addrman_info = at<8>;
    using get_connection_count = at<9>;
    using get_net_totals = at<10>;
    using get_node_addresses = at<11>;
    using get_peer_info = at<12>;
    using ping = at<13>;
    using set_network_active = at<14>;
};

/// The bitcoind handshake, published separately as it is served by its own
/// protocol (attached first), which claims negotiateversion alone.
struct bitcoind_handshake_methods
{
    static constexpr std::tuple methods
    {
        method<"negotiateversion", nullable<number_t>, nullable<number_t>, command>{ "minimum", "maximum" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    /// Method names as reported by help.
    static constexpr auto name_data = method_names<methods>();
    static constexpr std::string_view names{ name_data.data(),
        name_data.size() };

    template <size_t Index>
    using at = method_at<methods, Index>;

    using negotiate_version = at<0>;
};

namespace bitcoind
{
    /// Implemented interface versions, valued in the bitcoind version
    /// encoding (10'000 major, 100 minor, patch).
    enum class version : uint32_t
    {
        /// Invalid version.
        v0_0 = 0,

        /// bitcoind v30.0 (minimum).
        v30_0 = 300000,

        /// bitcoind v31.0.
        v31_0 = 310000,

        /// bitcoind v32.0 (maximum).
        v32_0 = 320000
    };

    /// The implemented versions, ascending.
    inline constexpr std::array versions
    {
        version::v30_0,
        version::v31_0,
        version::v32_0
    };
    inline constexpr uint32_t minimum_version = to_value(versions.front());
    inline constexpr uint32_t maximum_version = to_value(versions.back());

    /// A method and the version that introduced it.
    struct introduction
    {
        std::string_view name;
        version since;
    };

    /// The methods introduced after the minimum version, by name.
    inline constexpr std::array introductions
    {
        introduction{ "abortprivatebroadcast", version::v31_0 },
        introduction{ "getmempoolcluster", version::v31_0 },
        introduction{ "getmempoolfeeratediagram", version::v31_0 },
        introduction{ "getprivatebroadcastinfo", version::v31_0 },
        introduction{ "addhdkey", version::v32_0 },
        introduction{ "derivehdkey", version::v32_0 },
        introduction{ "exportasmap", version::v32_0 },
        introduction{ "exportwatchonlywallet", version::v32_0 },
        introduction{ "getopenrpcinfo", version::v32_0 },
        introduction{ "rpc.discover", version::v32_0 }
    };

    /// The version that introduced the method (the minimum if not listed).
    constexpr version introduced(std::string_view name) NOEXCEPT
    {
        for (const auto& entry: introductions)
            if (entry.name == name)
                return entry.since;

        return versions.front();
    }

    /// The greatest implemented version not exceeding value (v0_0 if none).
    constexpr version version_floor(uint32_t value) NOEXCEPT
    {
        auto floor = version::v0_0;
        for (const auto implemented: versions)
            if (to_value(implemented) <= value)
                floor = implemented;

        return floor;
    }

    /// The dotted form of a version (major.minor.patch).
    inline std::string to_string(version value) NOEXCEPT
    {
        constexpr uint32_t major = 10'000;
        constexpr uint32_t minor = 100;
        const auto number = to_value(value);
        return std::to_string(number / major) + "." +
            std::to_string((number % major) / minor) + "." +
            std::to_string(number % minor);
    }

    /// The bitcoind user agent form of a version.
    inline std::string to_subversion(version value) NOEXCEPT
    {
        return "/Satoshi:" + to_string(value) + "/";
    }
} // namespace bitcoind

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
