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
#ifndef LIBBITCOIN_SERVER_UTILITIES_COINBASE_HPP
#define LIBBITCOIN_SERVER_UTILITIES_COINBASE_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Largest output script of an address (p2wsh, p2tr).
constexpr size_t max_address_script = 34;

/// Largest single byte push (data size implied by opcode).
constexpr size_t max_direct_push = 75;

/// Bytes of the witness of a coinbase: marker, flag, count, size and the
/// reserved value (bip141).
constexpr size_t coinbase_witness_size = 2u + 1u + 1u + system::hash_size;

/// Bytes of the witness commitment output of a coinbase: value, script size,
/// op_return, push, head and commitment (bip141).
constexpr size_t coinbase_commitment_size = sizeof(uint64_t) + 1u + 2u +
    sizeof(uint32_t) + system::hash_size;

/// Size of a coinbase input script, the bip34 height at its largest followed
/// by the tag and extranonce each pushed by size (an empty tag is omitted).
BCS_API size_t coinbase_script_size(size_t tag, size_t extranonce) NOEXCEPT;

/// Weight of a coinbase with the given input script size, paying one output
/// script of the given size and committing to witnesses.
BCS_API size_t coinbase_weight(size_t script, size_t payout) NOEXCEPT;

/// Coinbase serialized without witness, through the extranonce push opcode.
BCS_API system::data_chunk coinbase_prefix(size_t height,
    const std::string& tag, size_t extranonce) NOEXCEPT;

/// Coinbase serialized without witness, following the extranonce.
BCS_API system::data_chunk coinbase_suffix(size_t height, uint64_t value,
    const system::chain::script& payout) NOEXCEPT;

/// Coinbase serialized without witness, following the extranonce, committing
/// to witnesses (bip141).
BCS_API system::data_chunk coinbase_suffix(size_t height, uint64_t value,
    const system::chain::script& payout,
    const system::hash_digest& commitment) NOEXCEPT;

/// Coinbase of prefix, extranonce and suffix, without witness.
BCS_API system::data_chunk coinbase_unwitnessed(
    const system::data_chunk& prefix, const system::data_chunk& extranonce,
    const system::data_chunk& suffix) NOEXCEPT;

/// Coinbase of prefix, extranonce and suffix, with witness (null reserved).
BCS_API system::data_chunk coinbase_witnessed(const system::data_chunk& prefix,
    const system::data_chunk& extranonce,
    const system::data_chunk& suffix) NOEXCEPT;

/// Merkle root of a block, from its coinbase txid and the coinbase branch.
BCS_API system::hash_digest coinbase_root(const system::hash_digest& txid,
    const system::hashes& branch) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
