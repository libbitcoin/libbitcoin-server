/*
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
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
    /// BTCD protocol (unversioned, json-rpc v1.0 over ws/s).
    static constexpr std::tuple methods
    {
        /// Handshake (disallowed when basic auth has been established).
        method<"authenticate", string_t, string_t>{ "username", "password" },

        /// Admin.
        method<"session">{},

        /// Block subscription.
        method<"notifyblocks">{},
        method<"stopnotifyblocks">{},

        /// Tx subscription.
        method<"notifynewtransactions", optional<false>>{ "verbose" },
        method<"stopnotifynewtransactions">{},

        /// Address/outpoint filtering.
        method<"loadtxfilter", boolean_t, value_t, value_t>{ "reload", "addresses", "outpoints" },
        method<"rescanblocks", value_t>{ "blockhashes" },

        /// Deprecated (address/outpoint filtering).
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
    using session = at<1>;
    using notify_blocks = at<2>;
    using stop_notify_blocks = at<3>;
    using notify_new_transactions = at<4>;
    using stop_notify_new_transactions = at<5>;
    using load_tx_filter = at<6>;
    using rescan_blocks = at<7>;
    using notify_received = at<8>;
    using stop_notify_received = at<9>;
    using notify_spent = at<10>;
    using stop_notify_spent = at<11>;
    using rescan = at<12>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
