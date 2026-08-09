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
        method<"setban">{ unimplemented },
        method<"addnode">{ unimplemented },
        method<"disconnectnode">{ unimplemented },
        method<"exportasmap">{ unimplemented },
        method<"getaddednodeinfo">{ unimplemented },
        method<"getaddrmaninfo">{ unimplemented },
        method<"getconnectioncount">{ unimplemented },
        method<"getnettotals">{},
        method<"getnodeaddresses">{ unimplemented },
        method<"getpeerinfo">{ unimplemented },
        method<"ping">{ unimplemented },
        method<"setnetworkactive">{ unimplemented }
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

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
