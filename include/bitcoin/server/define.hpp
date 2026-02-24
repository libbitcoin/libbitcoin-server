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
#ifndef LIBBITCOIN_SERVER_DEFINE_HPP
#define LIBBITCOIN_SERVER_DEFINE_HPP

#include <bitcoin/node.hpp>
#include <bitcoin/server/error.hpp>

/// Now we use the generic helper definitions above to define BCS_API
/// and BCS_INTERNAL. BCS_API is used for the public API symbols. It either DLL
/// imports or DLL exports (or does nothing for static build) BCS_INTERNAL is
/// used for non-api symbols.
#if defined BCS_STATIC
    #define BCS_API
    #define BCS_INTERNAL
#elif defined BCS_DLL
    #define BCS_API      BC_HELPER_DLL_EXPORT
    #define BCS_INTERNAL BC_HELPER_DLL_LOCAL
#else
    #define BCS_API      BC_HELPER_DLL_IMPORT
    #define BCS_INTERNAL BC_HELPER_DLL_LOCAL
#endif

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

// define.hpp is the common include for /server.
// All non-server headers include define.hpp.
// Server inclusions are chained as follows.

// version        : <generated>
// error          : version
// define         : error

// Other directory common includes are not internally chained.
// Each header includes only its required common headers.

// settings       : define
// configuration  : define settings
// parser         : define configuration
// /channels      : define configuration
// server_node    : define configuration
// session        : define                   [forward: server_node]
// /protocols     : define /channels         [session.hpp]
// /sessions      : define /protocols        [forward: server_node]