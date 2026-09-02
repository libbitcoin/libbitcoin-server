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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BTCD_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BTCD_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct btcd_methods
{
    /// BTCD protocol (json-rpc v2.0 over http/ws). Named arguments are
    /// required by v2.0, but btcd does not support them (supported here).
    static constexpr std::tuple methods
    {
        /// Administrative.
        method<"authenticate", string_t, string_t>{ "username", "password" },
        method<"session">{},
        method<"stop">{ unimplemented },

        /// Getters.
        method<"getbestblock">{},

        /// Overrides the bitcoind method, adding bip9_softforks (real btcd
        /// reports it, and lnd requires the taproot key to accept a backend).
        method<"getblockchaininfo">{},

        method<"getcfilter", string_t, number_t>{ "hash", "filtertype" },
        method<"getcfilterheader", string_t, number_t>{ "hash", "filtertype" },
        method<"getcurrentnet">{},
        method<"getdifficulty">{},
        method<"getheaders", array_t, string_t>{ "blocklocators", "hashstop" },
        method<"getinfo">{},
        method<"getnettotals">{},
        method<"searchrawtransactions", string_t, optional<1.0>, optional<0.0>, optional<100.0>, optional<0.0>, optional<false>, optional<empty::array>>{ "address", "verbose", "skip", "count", "vinextra", "reverse", "filteraddrs" },
        method<"version">{},

        /// Subscription.
        method<"notifyblocks">{},
        method<"stopnotifyblocks">{},
        method<"notifynewtransactions", optional<false>>{ unimplemented, "verbose" },
        method<"stopnotifynewtransactions">{ unimplemented },

        /// Filters.
        method<"loadtxfilter", boolean_t, value_t, value_t>{ "reload", "addresses", "outpoints" },
        method<"rescanblocks", value_t>{ "blockhashes" },

        /// Deprecated.
        method<"notifyreceived", value_t>{ unimplemented, "addresses" },
        method<"stopnotifyreceived", value_t>{ unimplemented, "addresses" },
        method<"notifyspent", value_t>{ unimplemented, "outpoints" },
        method<"stopnotifyspent", value_t>{ unimplemented, "outpoints" },
        method<"rescan", string_t, value_t, value_t, optional<""_t>>{ "beginblock", "addresses", "outpoints", "endblock" }
    };

    /// Method names as reported by help.
    static constexpr auto name_data = method_names<methods>();
    static constexpr std::string_view names{ name_data.data(),
        name_data.size() };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using authenticate = at<0>;
    using session = at<1>;
    using stop = at<2>;

    using get_best_block = at<3>;
    using get_block_chain_info = at<4>;
    using get_cfilter = at<5>;
    using get_cfilter_header = at<6>;
    using get_current_net = at<7>;
    using get_difficulty = at<8>;
    using get_headers = at<9>;
    using get_info = at<10>;
    using get_net_totals = at<11>;
    using search_raw_transactions = at<12>;
    using version = at<13>;

    using notify_blocks = at<14>;
    using stop_notify_blocks = at<15>;
    using notify_new_transactions = at<16>;
    using stop_notify_new_transactions = at<17>;

    using load_tx_filter = at<18>;
    using rescan_blocks = at<19>;

    using notify_received = at<20>;
    using stop_notify_received = at<21>;
    using notify_spent = at<22>;
    using stop_notify_spent = at<23>;
    using rescan = at<24>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
