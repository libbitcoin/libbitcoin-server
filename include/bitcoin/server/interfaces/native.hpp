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
#ifndef LIBBITCOIN_SERVER_INTERFACES_NATIVE_HPP
#define LIBBITCOIN_SERVER_INTERFACES_NATIVE_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct native_methods
{
    static constexpr std::tuple methods
    {
        method<"configuration", uint8_t, uint8_t>{ "version", "media" },

        method<"top", uint8_t, uint8_t>{ "version", "media" },
        method<"block", uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>, optional<true>>{ "version", "media", "hash", "height", "witness" },
        method<"block_header", uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "hash", "height" },
        method<"block_header_context", uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "hash", "height" },
        method<"block_details", uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "hash", "height" },
        method<"block_txs", uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "hash", "height" },
        method<"block_filter", uint8_t, uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "type", "hash", "height" },
        method<"block_filter_hash", uint8_t, uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "type", "hash", "height" },
        method<"block_filter_header", uint8_t, uint8_t, uint8_t, nullable<system::hash_cptr>, nullable<uint32_t>>{ "version", "media", "type", "hash", "height" },
        method<"block_tx", uint8_t, uint8_t, uint32_t, nullable<system::hash_cptr>, nullable<uint32_t>, optional<true>>{ "version", "media", "position", "hash", "height", "witness" },

        method<"tx", uint8_t, uint8_t, system::hash_cptr, optional<true>>{ "version", "media", "hash", "witness" },
        method<"tx_header", uint8_t, uint8_t, system::hash_cptr>{ "version", "media", "hash" },
        method<"tx_details", uint8_t, uint8_t, system::hash_cptr>{ "version", "media", "hash" },

        method<"inputs", uint8_t, uint8_t, system::hash_cptr, optional<true>>{ "version", "media", "hash", "witness" },
        method<"input", uint8_t, uint8_t, system::hash_cptr, uint32_t, optional<true>>{ "version", "media", "hash", "index", "witness" },
        method<"input_script", uint8_t, uint8_t, system::hash_cptr, uint32_t>{ "version", "media", "hash", "index" },
        method<"input_witness", uint8_t, uint8_t, system::hash_cptr, uint32_t>{ "version", "media", "hash", "index" },
 
        method<"outputs", uint8_t, uint8_t, system::hash_cptr>{ "version", "media", "hash" },
        method<"output", uint8_t, uint8_t, system::hash_cptr, uint32_t>{ "version", "media", "hash", "index" },
        method<"output_script", uint8_t, uint8_t, system::hash_cptr, uint32_t>{ "version", "media", "hash", "index" },
        method<"output_spender", uint8_t, uint8_t, system::hash_cptr, uint32_t>{ "version", "media", "hash", "index" },
        method<"output_spenders", uint8_t, uint8_t, system::hash_cptr, uint32_t>{ "version", "media", "hash", "index" },

        method<"address", uint8_t, uint8_t, system::hash_cptr, optional<true>>{ "version", "media", "hash", "turbo" },
        method<"address_confirmed", uint8_t, uint8_t, system::hash_cptr, optional<true>>{ "version", "media", "hash", "turbo" },
        method<"address_unconfirmed", uint8_t, uint8_t, system::hash_cptr, optional<true>>{ "version", "media", "hash", "turbo" },
        method<"address_balance", uint8_t, uint8_t, system::hash_cptr, optional<true>>{ "version", "media", "hash", "turbo" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.

    using configuration = at<0>;

    using top = at<1>;

    using block = at<2>;
    using block_header = at<3>;
    using block_header_context = at<4>;
    using block_details = at<5>;
    using block_txs = at<6>;
    using block_filter = at<7>;
    using block_filter_hash = at<8>;
    using block_filter_header = at<9>;
    using block_tx = at<10>;

    using tx = at<11>;
    using tx_header = at<12>;
    using tx_details = at<13>;

    using inputs = at<14>;
    using input = at<15>;
    using input_script = at<16>;
    using input_witness = at<17>;

    using outputs = at<18>;
    using output = at<19>;
    using output_script = at<20>;
    using output_spender = at<21>;
    using output_spenders = at<22>;

    using address = at<23>;
    using address_confirmed = at<24>;
    using address_unconfirmed = at<25>;
    using address_balance = at<26>;
};

/// ?format=data|text|json (via query string).
/// -----------------------------------------------------------------------

/// /v1/configuration {1}

/// /v1/top {1}

/// /v1/block/hash/[bkhash] {1}
/// /v1/block/height/[height] {1}

/// /v1/block/hash/[bkhash]/header {1}
/// /v1/block/height/[height]/header {1}

/// /v1/block/hash/[bkhash]/header/context {1}
/// /v1/block/height/[height]/header/context {1}

/// /v1/block/hash/[bkhash]/details {1}
/// /v1/block/height/[height]/details {1}

/// /v1/block/hash/[bkhash]/filter/[type] {1}
/// /v1/block/height/[height]/filter/[type] {1}

/// /v1/block/hash/[bkhash]/filter/[type]/hash {1}
/// /v1/block/height/[height]/filter/[type]/hash {1}

/// /v1/block/hash/[bkhash]/filter/[type]/header {1}
/// /v1/block/height/[height]/filter/[type]/header {1}

/// /v1/block/hash/[bkhash]/txs {all txs in the block}
/// /v1/block/height/[height]/txs {all txs in the block}

/// /v1/block/hash/[bkhash]/tx/[position] {1}
/// /v1/block/height/[height]/tx/[position] {1}

/// -----------------------------------------------------------------------

/// /v1/tx/[txhash] {1}
/// /v1/tx/[txhash]/header {1 - if confirmed}
/// /v1/tx/[txhash]/details {1}

/// -----------------------------------------------------------------------

/// /v1/input/[txhash] {all inputs in the tx}
/// /v1/input/[txhash]/[index] {1}
/// /v1/input/[txhash]/[index]/script {1}
/// /v1/input/[txhash]/[index]/witness {1}

/// -----------------------------------------------------------------------

/// /v1/output/[txhash] {all outputs in the tx}
/// /v1/output/[txhash]/[index] {1}
/// /v1/output/[txhash]/[index]/script {1}
/// /v1/output/[txhash]/[index]/spender {1 - if confirmed}
/// /v1/output/[txhash]/[index]/spenders {all}

/// -----------------------------------------------------------------------

/// /v1/address/[output-script-hash] {all}
/// /v1/address/[output-script-hash]/unconfirmed {all unconfirmed}
/// /v1/address/[output-script-hash]/confirmed {all unconfirmed}
/// /v1/address/[output-script-hash]/balance {all confirmed}

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
