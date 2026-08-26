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
#include <bitcoin/server/serializers/bitcoind_coin.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void to_coin_data(data_chunk& out,
    const database::unspent_coin& coin) NOEXCEPT
{
    constexpr auto overhead = hash_size + sizeof(uint32_t) + sizeof(uint32_t) +
        sizeof(uint64_t);

    out.resize(overhead + variable_size(coin.script.size()) +
        coin.script.size());
    stream::out::fast ostream(out);
    write::bytes::fast sink(ostream);
    sink.write_bytes(coin.txid);
    sink.write_4_bytes_little_endian(coin.index);
    sink.write_4_bytes_little_endian(bit_or(shift_left(
        possible_narrow_cast<uint32_t>(coin.height), 1),
        to_int<uint32_t>(coin.coinbase)));
    sink.write_8_bytes_little_endian(coin.value);
    sink.write_variable(coin.script.size());
    sink.write_bytes(coin.script);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
