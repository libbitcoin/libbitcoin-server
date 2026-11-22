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
#ifndef LIBBITCOIN_SERVER_UTILITIES_MERKLE_HPP
#define LIBBITCOIN_SERVER_UTILITIES_MERKLE_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// bip37 partial merkle tree, as carried by a merkle block. Tree construction;
/// merkle block that wraps it is wire form used by bitcoind's
/// gettxoutproof/verifytxoutproof and p2p merkleblock message.

/// Build flags and branch hashes proving matched txids within a block whose
/// full txid list (in block order) is given. match[i] flags txids[i]. Requires
/// match.size() == txids.size() and non-empty txids.
BCS_API void build_partial_merkle(system::data_chunk& flags,
    system::hashes& branch, const system::hashes& txids,
    const std::vector<bool>& match) NOEXCEPT;

/// Extract merkle root and matched txids (with their positions) from a partial
/// merkle tree of given transaction count. Returns false if tree is malformed,
/// in which case outputs are unspecified. True return with root matching
/// block's merkle root proves matched txids.
BCS_API bool extract_partial_merkle(system::hash_digest& root,
    system::hashes& matched, std::vector<size_t>& positions, size_t txs,
    const system::data_chunk& flags, const system::hashes& branch) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
