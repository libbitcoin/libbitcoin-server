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
#ifndef LIBBITCOIN_SERVER_INTERFACES_ESPLORA_HPP
#define LIBBITCOIN_SERVER_INTERFACES_ESPLORA_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct esplora_methods
{
    // github.com/Blockstream/esplora/blob/master/API.md
    static constexpr std::tuple methods
    {
        method<"tx", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"tx_status", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"tx_merkleblock_proof", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"tx_merkle_proof", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"tx_outspend", uint8_t, system::hash_cptr, uint32_t>{ "media", "hash", "index" },
        method<"tx_outspends", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"broadcast", uint8_t, string_t>{ "media", "transaction" },
        method<"broadcast_package", uint8_t, array_t>{ unimplemented, "media", "transactions" },

        method<"address", uint8_t, nullable<system::hash_cptr>, nullable<string_t>>{ "media", "hash", "address" },
        method<"address_txs", uint8_t, nullable<system::hash_cptr>, nullable<string_t>>{ "media", "hash", "address" },
        method<"address_txs_chain", uint8_t, nullable<system::hash_cptr>, nullable<string_t>, nullable<system::hash_cptr>>{ "media", "hash", "address", "last_seen" },
        method<"address_txs_mempool", uint8_t, nullable<system::hash_cptr>, nullable<string_t>>{ "media", "hash", "address" },
        method<"address_utxo", uint8_t, nullable<system::hash_cptr>, nullable<string_t>>{ "media", "hash", "address" },

        method<"block", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"block_header", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"block_status", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"block_txs", uint8_t, system::hash_cptr, optional<0_u32>>{ "media", "hash", "start" },
        method<"block_txids", uint8_t, system::hash_cptr>{ "media", "hash" },
        method<"block_txid", uint8_t, system::hash_cptr, uint32_t>{ "media", "hash", "index" },
        method<"block_height", uint8_t, uint32_t>{ "media", "height" },
        method<"blocks", uint8_t, nullable<uint32_t>>{ "media", "height" },
        method<"tip_height", uint8_t>{ "media" },
        method<"tip_hash", uint8_t>{ "media" },

        method<"mempool", uint8_t>{ "media" },
        method<"mempool_txids", uint8_t>{ "media" },
        method<"mempool_recent", uint8_t>{ "media" },

        method<"fee_estimates", uint8_t>{ "media" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.

    using tx = at<0>;
    using tx_status = at<1>;
    using tx_merkleblock_proof = at<2>;
    using tx_merkle_proof = at<3>;
    using tx_outspend = at<4>;
    using tx_outspends = at<5>;
    using broadcast = at<6>;
    using broadcast_package = at<7>;

    using address = at<8>;
    using address_txs = at<9>;
    using address_txs_chain = at<10>;
    using address_txs_mempool = at<11>;
    using address_utxo = at<12>;

    using block = at<13>;
    using block_header = at<14>;
    using block_status = at<15>;
    using block_txs = at<16>;
    using block_txids = at<17>;
    using block_txid = at<18>;
    using block_height = at<19>;
    using blocks = at<20>;
    using tip_height = at<21>;
    using tip_hash = at<22>;

    using mempool = at<23>;
    using mempool_txids = at<24>;
    using mempool_recent = at<25>;

    using fee_estimates = at<26>;
};

/// media is implied by the target (there is no format query string).
/// ---------------------------------------------------------------------------

/// /tx/[txhash] {1}
/// /tx/[txhash]/hex {1, text}
/// /tx/[txhash]/raw {1, data}
/// /tx/[txhash]/status {1}
/// /tx/[txhash]/merkleblock-proof {1, text}
/// /tx/[txhash]/merkle-proof {1}
/// /tx/[txhash]/outspend/[index] {1}
/// /tx/[txhash]/outspends {all outputs in the tx}
/// /tx {txhash, text} [post, body is transaction hex]
/// /txs/package {all} [post, body is array of transaction hex]

/// ---------------------------------------------------------------------------

/// /address/[address] {1}
/// /scripthash/[output-script-hash] {1}
/// /address/[address]/txs {25 confirmed and 50 unconfirmed}
/// /address/[address]/txs/chain {25 confirmed}
/// /address/[address]/txs/chain/[last-seen-txhash] {25 confirmed}
/// /address/[address]/txs/mempool {50 unconfirmed}
/// /address/[address]/utxo {all}

/// ---------------------------------------------------------------------------

/// /block/[bkhash] {1}
/// /block/[bkhash]/raw {1, data}
/// /block/[bkhash]/header {1, text}
/// /block/[bkhash]/status {1}
/// /block/[bkhash]/txs {25 txs in the block}
/// /block/[bkhash]/txs/[start-index] {25 txs in the block}
/// /block/[bkhash]/txids {all txs in the block}
/// /block/[bkhash]/txid/[index] {1, text}
/// /block-height/[height] {bkhash, text}
/// /blocks {10 from the top}
/// /blocks/[height] {10 from the height}
/// /blocks/tip/height {1, text}
/// /blocks/tip/hash {1, text}

/// ---------------------------------------------------------------------------

/// /mempool {1}
/// /mempool/txids {all}
/// /mempool/recent {10}

/// /fee-estimates {1}

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
