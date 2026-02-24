/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_NATIVE_IPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_NATIVE_IPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

template <typename Object, typename ...Args>
inline system::data_chunk protocol_native::to_bin(const Object& object,
    size_t size, Args&&... args) NOEXCEPT
{
    using namespace system;
    data_chunk out(size);
    stream::out::fast sink{ out };
    write::bytes::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    BC_ASSERT(writer);
    return out;
}

template <typename Object, typename ...Args>
inline std::string protocol_native::to_hex(const Object& object, size_t size,
    Args&&... args) NOEXCEPT
{
    using namespace system;
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    BC_ASSERT(writer);
    return out;
}

template <typename Collection, typename ...Args>
inline system::data_chunk protocol_native::to_bin_array(
    const Collection& collection, size_t size, Args&&... args) NOEXCEPT
{
    using namespace system;
    data_chunk out(size);
    stream::out::fast sink{ out };
    write::bytes::fast writer{ sink };
    for (const auto& element: collection)
        element.to_data(writer, std::forward<Args>(args)...);

    BC_ASSERT(writer);
    return out;
}

template <typename Collection, typename ...Args>
inline std::string protocol_native::to_hex_array(const Collection& collection,
    size_t size, Args&&... args) NOEXCEPT
{
    using namespace system;
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    for (const auto& element: collection)
        element.to_data(writer, std::forward<Args>(args)...);

    BC_ASSERT(writer);
    return out;
}

template <typename Collection, typename ...Args>
inline system::data_chunk protocol_native::to_bin_ptr_array(
    const Collection& collection, size_t size, Args&&... args) NOEXCEPT
{
    using namespace system;
    data_chunk out(size);
    stream::out::fast sink{ out };
    write::bytes::fast writer{ sink };
    for (const auto& ptr: collection)
        ptr->to_data(writer, std::forward<Args>(args)...);

    BC_ASSERT(writer);
    return out;
}

template <typename Collection, typename ...Args>
inline std::string protocol_native::to_hex_ptr_array(
    const Collection& collection, size_t size, Args&&... args) NOEXCEPT
{
    using namespace system;
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    for (const auto& ptr: collection)
        ptr->to_data(writer, std::forward<Args>(args)...);

    BC_ASSERT(writer);
    return out;
}

} // namespace server
} // namespace libbitcoin

#endif
