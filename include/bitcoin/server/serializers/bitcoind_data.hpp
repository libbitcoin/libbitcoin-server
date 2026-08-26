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
#ifndef LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_DATA_HPP
#define LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_DATA_HPP

#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Serialize an object (chain::header, chain::transaction, ...) to bytes.
template <typename Object, typename ...Args>
system::data_chunk to_data(const Object& object, size_t size,
    Args&&... args) NOEXCEPT
{
    system::data_chunk out(size);
    system::stream::out::fast sink{ out };
    system::write::bytes::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    BC_ASSERT(writer);
    return out;
}

/// Serialize an object (chain::header, chain::transaction, ...) to a
/// base16 string.
template <typename Object, typename ...Args>
std::string to_text(const Object& object, size_t size,
    Args&&... args) NOEXCEPT
{
    std::string out(two * size, '\0');
    system::stream::out::fast sink{ out };
    system::write::base16::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    BC_ASSERT(writer);
    return out;
}

} // namespace server
} // namespace libbitcoin

#endif
