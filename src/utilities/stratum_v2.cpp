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
#include <bitcoin/server/utilities/stratum_v2.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/stratum_v1.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

hash_digest to_target_hash(double difficulty) NOEXCEPT
{
    return from_uintx(to_share_target(difficulty));
}

double to_hash_difficulty(const hash_digest& target) NOEXCEPT
{
    return to_share_difficulty(to_uintx(target));
}

// A share of difficulty one is found once in 2^32 hashes.
double to_rate_difficulty(double hash_rate, uint32_t seconds) NOEXCEPT
{
    if (!(hash_rate > 0.0) || std::isinf(hash_rate) || is_zero(seconds))
        return 0.0;

    return std::ldexp(hash_rate * seconds, -32);
}

} // namespace server
} // namespace libbitcoin
