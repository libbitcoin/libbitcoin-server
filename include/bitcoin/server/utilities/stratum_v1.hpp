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
#ifndef LIBBITCOIN_SERVER_UTILITIES_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_UTILITIES_STRATUM_V1_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// General purpose header version bits a miner may roll (bip320).
constexpr uint32_t version_rolling_mask = 0x1fffe000;

/// Previous block hash of a job, each four byte word of the header field
/// reversed and encoded as base16.
BCS_API std::string to_stratum_hash(const system::hash_digest& hash) NOEXCEPT;

/// A word as eight base16 characters, most significant first.
BCS_API std::string encode_word(uint32_t value) NOEXCEPT;

/// Decode eight base16 characters, most significant first.
BCS_API bool decode_word(uint32_t& out, const std::string& text) NOEXCEPT;

/// Share target of a difficulty, relative to the target of difficulty one.
BCS_API uint256_t to_share_target(double difficulty) NOEXCEPT;

/// Difficulty of a share target, relative to the target of difficulty one.
BCS_API double to_share_difficulty(const uint256_t& target) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
