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
#ifndef LIBBITCOIN_SERVER_UTILITIES_BITCOIND_MERKLE_HPP
#define LIBBITCOIN_SERVER_UTILITIES_BITCOIND_MERKLE_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The bip37 partial merkle tree, as carried by a merkle block. This is the
/// tree construction; the merkle block that wraps it is the wire form used by
/// bitcoind's gettxoutproof/verifytxoutproof and the p2p merkleblock message.

/// Build the flags and branch hashes proving the matched txids within a block
/// whose full txid list (in block order) is given. match[i] flags txids[i].
/// Requires match.size() == txids.size() and a non-empty txids.
BCS_API void build_partial_merkle(system::data_chunk& flags,
    system::hashes& branch, const system::hashes& txids,
    const std::vector<bool>& match) NOEXCEPT;

/// Extract the merkle root and matched txids (with their positions) from a
/// partial merkle tree of the given transaction count. Returns false if the
/// tree is malformed, in which case outputs are unspecified. A true return
/// with a root matching the block's merkle root proves the matched txids.
BCS_API bool extract_partial_merkle(system::hash_digest& root,
    system::hashes& matched, std::vector<size_t>& positions, size_t txs,
    const system::data_chunk& flags, const system::hashes& branch) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
