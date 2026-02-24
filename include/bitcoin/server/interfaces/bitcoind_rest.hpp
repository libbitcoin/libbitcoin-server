/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_REST_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_REST_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_rest_methods
{
    // github.com/bitcoin/bitcoin/blob/master/doc/REST-interface.md
    static constexpr std::tuple methods
    {
        // blocks (block_part is bin|hex only)
        method<"block", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"block_hash", uint8_t, uint32_t>{ "media", "height" },
        method<"block_txs", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"block_headers", uint8_t, system::hash_cptr, uint32_t>{ "media", "hash", "count" },
        method<"block_part", uint8_t, system::hash_cptr, uint32_t, uint32_t>{ "media", "hash", "offset", "size" },
        method<"block_spent_tx_outputs", uint8_t, system::hash_cptr>{ "media", "hash" },

        // client filters
        method<"block_filter", uint8_t, system::hash_cptr, uint8_t>{ "media", "hash", "type" },
        method<"block_filter_headers", uint8_t, system::hash_cptr, uint8_t>{ "media", "hash", "type" },

        // unspent outputs
        method<"get_utxos", uint8_t, system::hash_cptr, uint8_t>{ "media", "hash", "type" },
        method<"get_utxos_confirmed", uint8_t, system::hash_cptr, uint8_t>{ "media", "hash", "type" },

        // mempool (json only)
        method<"mempool", optional<true>, optional<false>>{ "verbose", "sequence" },

        // info (json only)
        method<"chain_information">{},
        method<"mempool_information">{},
        method<"fork_information", nullable<system::hash_cptr>>{ "hash" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using block = at<0>;
    using block_hash = at<1>;
    using block_txs = at<2>;
    using block_headers = at<3>;
    using block_part = at<4>;
    using block_spent_tx_outputs = at<5>;
    using block_filter = at<6>;
    using block_filter_headers = at<7>;
    using get_utxos = at<8>;
    using get_utxos_confirmed = at<9>;
    using mempool = at<10>;
    using chain_information = at<11>;
    using mempool_information = at<12>;
    using fork_information = at<13>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
