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
#ifndef LIBBITCOIN_SERVER_DEFINE_HPP
#define LIBBITCOIN_SERVER_DEFINE_HPP

#include <bitcoin/node.hpp>
#include <bitcoin/server/error/error.hpp>

/// Augment limited xcode placeholder defines (10 vs. common 20).
/// ---------------------------------------------------------------------------
#if defined (HAVE_XCODE)

/// Define custom placeholder types in the global namespace to avoid conflicts.
struct placeholder_11 {};
struct placeholder_12 {};
struct placeholder_13 {};

/// Specialize std::is_placeholder within the std namespace.
namespace std
{
    template <>
    struct is_placeholder<::placeholder_11> : integral_constant<int, 11> {};
    template <>
    struct is_placeholder<::placeholder_12> : integral_constant<int, 12> {};
    template <>
    struct is_placeholder<::placeholder_13> : integral_constant<int, 13> {};
}

/// Add instances to std::placeholders for standard usage syntax.
namespace std::placeholders
{
    inline constexpr ::placeholder_11 _11{};
    inline constexpr ::placeholder_12 _12{};
    inline constexpr ::placeholder_13 _13{};
}

#endif // HAVE_XCODE
/// ---------------------------------------------------------------------------

#endif
