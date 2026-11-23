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
#include <bitcoin/server/utilities/stratum_v1.hpp>

#include <cmath>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace system::chain;

// The target of difficulty one (bits 0x1d00ffff), below 2^224.
static uint256_t difficulty_one() NOEXCEPT
{
    return chain::compact::expand(0x1d00ffff);
}

std::string to_stratum_hash(const hash_digest& hash) NOEXCEPT
{
    constexpr auto word = sizeof(uint32_t);
    auto out = hash;
    for (auto it = out.begin(); it != out.end(); std::advance(it, word))
        std::reverse(it, std::next(it, word));

    return encode_base16(out);
}

std::string encode_word(uint32_t value) NOEXCEPT
{
    return encode_base16(to_big_endian(value));
}

bool decode_word(uint32_t& out, const std::string& text) NOEXCEPT
{
    data_array<sizeof(uint32_t)> bytes{};
    if (!decode_base16(bytes, text))
        return false;

    out = from_big_endian<uint32_t>(bytes);
    return true;
}

// The difficulty is divided as a 32 bit mantissa and a binary exponent, so
// the shifted target of difficulty one stays below 2^256. The target
// saturates for a difficulty below that of the largest target.
uint256_t to_share_target(double difficulty) NOEXCEPT
{
    constexpr auto precision = 32;
    if (!(difficulty > 0.0) || std::isinf(difficulty))
        return std::isinf(difficulty) ? uint256_t{} : ~uint256_t{};

    int exponent{};
    const auto mantissa = std::frexp(difficulty, &exponent);
    if (exponent < -(precision - 1))
        return ~uint256_t{};

    const auto scaled = static_cast<uint64_t>(std::ldexp(mantissa, precision));
    const auto target = (difficulty_one() << precision) / scaled;
    return exponent > 0 ? target >> exponent : target << -exponent;
}

double to_share_difficulty(const uint256_t& target) NOEXCEPT
{
    if (is_zero(target))
        return 0.0;

    return difficulty_one().convert_to<double>() /
        target.convert_to<double>();
}

} // namespace server
} // namespace libbitcoin
