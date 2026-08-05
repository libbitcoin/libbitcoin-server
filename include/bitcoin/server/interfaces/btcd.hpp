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
    /// BTCD protocol (unversioned, json-rpc v1.0 over http/ws).
    static constexpr std::tuple methods
    {
        /// Administrative (stop not implemented).
        method<"authenticate", string_t, string_t>{ "username", "password" },
        method<"help", optional<""_t>>{ "command" },
        method<"session">{},
        method<"stop">{},

        /// Getters.
        method<"getbestblock">{},
        method<"getcurrentnet">{},
        method<"getdifficulty">{},
        method<"getinfo">{},
        method<"getnettotals">{},
        method<"getnetworkhashps", optional<120_u32>, optional<-1_i32>>{ "blocks", "height" },

        /// Tools.
        method<"createrawtransaction", array_t, object_t, optional<0_u32>>{ "inputs", "outputs", "locktime" },
        method<"decoderawtransaction", string_t>{ "hexstring" },
        method<"decodescript", string_t>{ "hex" },
        method<"validateaddress", string_t>{ "address" },

        /// Subscription.
        method<"notifyblocks">{},
        method<"stopnotifyblocks">{},
        method<"notifynewtransactions", optional<false>>{ "verbose" },
        method<"stopnotifynewtransactions">{},

        /// Filters.
        method<"loadtxfilter", boolean_t, value_t, value_t>{ "reload", "addresses", "outpoints" },
        method<"rescanblocks", value_t>{ "blockhashes" },

        /// Deprecated.
        method<"notifyreceived", value_t>{ "addresses" },
        method<"stopnotifyreceived", value_t>{ "addresses" },
        method<"notifyspent", value_t>{ "outpoints" },
        method<"stopnotifyspent", value_t>{ "outpoints" },
        method<"rescan", string_t, value_t, value_t, optional<""_t>>{ "beginblock", "addresses", "outpoints", "endblock" }
    };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using authenticate = at<0>;
    using help = at<1>;
    using session = at<2>;
    using stop = at<3>;

    using get_best_block = at<4>;
    using get_current_net = at<5>;
    using get_difficulty = at<6>;
    using get_info = at<7>;
    using get_net_totals = at<8>;
    using get_network_hash_ps = at<9>;

    using create_raw_transaction = at<10>;
    using decode_raw_transaction = at<11>;
    using decode_script = at<12>;
    using validate_address = at<13>;

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
