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
#ifndef LIBBITCOIN_SERVER_UTILITIES_ELECTRUM_VERSION_HPP
#define LIBBITCOIN_SERVER_UTILITIES_ELECTRUM_VERSION_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {
namespace electrum {

enum class version
{
    /// Invalid version.
    v0_0,

    /// 2011, initial protocol negotiation (unsupported).
    v0_6,

    /// 2012, enhanced protocol negotiation (unsupported).
    v0_8,

    /// 2012, added pruning limits and transport indicators (unsupported).
    v0_9,

    /// 2013, baseline for core methods in official specification (unsupported).
    v0_10,

    /// 2014, deprecations of utxo and block number methods (minimum).
    v1_0,

    /// 2015, updated version response and introduced scripthash methods.
    v1_1,

    /// 2017, added optional parameters for transactions and headers.
    v1_2,

    /// 2018, defaulted raw headers and introduced new block methods.
    v1_3,

    /// 2019, removed deserialized headers and added merkle proof features.
    v1_4,

    /// 2019, modifications for auxiliary proof-of-work handling
    /// (unsupported).
    /// Version number allowed but no difference from v1.4.
    v1_4_1,

    /// 2020, added scripthash unsubscribe functionality.
    v1_4_2,

    /// Name index coins only (e.g. Namecoin) (unsupported).
    //v1_4_3,

    /// There is no v1.5 release (skipped).
    //v1_5,

    /// 2022, updated response formats, added fee estimation modes.
    v1_6,

    /// Adds outpoint, scriptpubkey and mempool methods (maximum).
    v1_7
};


/// The numeric form of a defined version (0.0 if undefined).
BCS_API system::config::version version_to_number(version value) NOEXCEPT;

/// Serialized form of a defined version ("0.0" if undefined).
BCS_API std::string version_to_string(version value) NOEXCEPT;

/// Parse any dotted numeric version, defined or not (false if malformed).
BCS_API bool version_from_string(system::config::version& out,
    const std::string_view& value) NOEXCEPT;

/// The greatest defined version not exceeding value (v0_0 if none).
BCS_API version version_floor(const system::config::version& value) NOEXCEPT;

/// Parse a requested version (string, or [min, max] array) to its range.
bool version_range(system::config::version& min, system::config::version& max,
    const network::rpc::value_t& value) NOEXCEPT;

/// Negotiate the requested version within the configured range, v0_0 if none.
version negotiate(const network::rpc::value_t& value,
    const system::config::version& minimum,
    const system::config::version& maximum) NOEXCEPT;

/// The greatest version permitting a non-version opener (below 1.6).
inline constexpr version restricted_maximum = version::v1_4_2;

/// Client names are informational and truncated to this length.
inline constexpr size_t maximum_client_name = 1024;

/// Escape a client name for logging (ascii, no whitespace).
std::string escape_client(const std::string& in) NOEXCEPT;

/// The server.version result for the negotiated version.
network::rpc::value_t version_result(version negotiated,
    const std::string& server_name) NOEXCEPT;

} // namespace electrum
} // namespace server
} // namespace libbitcoin

#endif
