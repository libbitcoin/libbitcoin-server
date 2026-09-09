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
#ifndef LIBBITCOIN_SERVER_INTERFACES_SPARROW_HPP
#define LIBBITCOIN_SERVER_INTERFACES_SPARROW_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

/// Methods served in addition to electrum, as implemented by frigate and
/// consumed by sparrow. Not electrum protocol methods, so not versioned by
/// the electrum handshake.
struct sparrow_methods
{
    static constexpr std::tuple methods
    {
        /// Block statistics (as the bitcoind getblockstats subset).
        method<"blockchain.block.stats", number_t>{ "height" },

        /// Silent payment (bip352) scanning, one subscription per address.
        method<"blockchain.silentpayments.subscribe", string_t, string_t, optional<empty::value>, optional<empty::array>>{ "scan_private_key", "spend_public_key", "start", "labels" },
        method<"blockchain.silentpayments.unsubscribe", string_t, string_t>{ "scan_private_key", "spend_public_key" }
    };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    using blockchain_block_stats = at<0>;
    using blockchain_silent_payments_subscribe = at<1>;
    using blockchain_silent_payments_unsubscribe = at<2>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
