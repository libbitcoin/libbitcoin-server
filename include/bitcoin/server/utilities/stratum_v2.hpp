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
#ifndef LIBBITCOIN_SERVER_UTILITIES_STRATUM_V2_HPP
#define LIBBITCOIN_SERVER_UTILITIES_STRATUM_V2_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Share target of a difficulty, as a little endian u256.
BCS_API system::hash_digest to_target_hash(double difficulty) NOEXCEPT;

/// Difficulty of a little endian u256 share target, zero if zero.
BCS_API double to_hash_difficulty(const system::hash_digest& target) NOEXCEPT;

/// Difficulty of one share per interval at the hash rate, zero if invalid.
BCS_API double to_rate_difficulty(double hash_rate, uint32_t seconds) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
