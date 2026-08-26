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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_CONTROL_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_CONTROL_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_control_methods
{
    static constexpr std::tuple methods
    {
        method<"help", optional<""_t>>{ "command" },
        method<"stop">{ unimplemented },
        method<"getmemoryinfo", optional<"stats"_t>>{ "mode" },
        method<"getopenrpcinfo", optional<false>>{ "show_hidden" },
        method<"getrpcinfo">{},
        method<"logging", optional<empty::array>, optional<empty::array>>{ "include", "exclude" },
        method<"uptime">{},
        method<"rpc.discover", optional<false>>{ "show_hidden" }
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
    using help = at<0>;
    using stop = at<1>;
    using get_memory_info = at<2>;
    using get_openrpc_info = at<3>;
    using get_rpc_info = at<4>;
    using logging = at<5>;
    using uptime = at<6>;
    using rpc_discover = at<7>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
