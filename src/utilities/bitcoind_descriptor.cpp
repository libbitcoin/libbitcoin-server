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
#include <bitcoin/server/utilities/bitcoind_descriptor.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

// The bip380 checksum generator (bch code over GF(32), as bech32).
static uint64_t poly_mod(uint64_t check, uint64_t value) NOEXCEPT
{
    auto out = bit_xor(shift_left(bit_and(check, 0x00000007ffffffff_u64), 5),
        value);

    if (get_right(check, 35)) out = bit_xor(out, 0xf5dee51989_u64);
    if (get_right(check, 36)) out = bit_xor(out, 0xa9fdca3312_u64);
    if (get_right(check, 37)) out = bit_xor(out, 0x1bab10e32d_u64);
    if (get_right(check, 38)) out = bit_xor(out, 0x3706b1677a_u64);
    if (get_right(check, 39)) out = bit_xor(out, 0x644d626ffd_u64);
    return out;
}

std::string descriptor_checksum(const std::string& descriptor) NOEXCEPT
{
    // Grouped so that common case and keypath errors cost a single symbol.
    static const std::string input_charset
    {
        "0123456789()[],'/*abcdefgh@:$%{}"
        "IJKLMNOPQRSTUVWXYZ&+-.;<=>?!^_|~"
        "ijklmnopqrstuvwxyzABCDEFGH`#\"\\ "
    };

    // The checksum character set (as bech32).
    static const std::string checksum_charset
    {
        "qpzry9x8gf2tvdw0s3jn54khce6mua7l"
    };

    uint64_t check = 1;
    uint64_t group{};
    size_t grouped{};
    for (const auto character: descriptor)
    {
        const auto position = input_charset.find(character);
        if (position == std::string::npos)
            return {};

        // A symbol for the position in the group of each character, and one
        // for the group numbers of each three characters.
        check = poly_mod(check, bit_and<uint64_t>(position, 31));
        group = 3 * group + shift_right(position, 5);
        if (++grouped == 3)
        {
            check = poly_mod(check, group);
            group = 0;
            grouped = 0;
        }
    }

    if (is_nonzero(grouped))
        check = poly_mod(check, group);

    // Shift out the checksum, ensuring appended zeroes affect it.
    for (size_t pad{}; pad < 8; ++pad)
        check = poly_mod(check, 0);

    check = bit_xor<uint64_t>(check, 1);

    std::string out(8, ' ');
    for (size_t index{}; index < 8; ++index)
        out.at(index) = checksum_charset.at(bit_and<uint64_t>(
            shift_right(check, 5 * (7 - index)), 31));

    return out;
}

} // namespace server
} // namespace libbitcoin
