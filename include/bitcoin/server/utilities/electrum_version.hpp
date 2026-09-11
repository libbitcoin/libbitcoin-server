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

/// The numeric form of a defined version (0.0 if undefined).
system::config::version version_to_number(version value) NOEXCEPT;

/// Serialized form of a defined version ("0.0" if undefined).
std::string version_to_string(version value) NOEXCEPT;

/// Parse any dotted numeric version, defined or not (false if malformed).
bool version_from_string(system::config::version& out,
    const std::string_view& value) NOEXCEPT;

/// The greatest defined version not exceeding value (v0_0 if none).
version version_floor(const system::config::version& value) NOEXCEPT;

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
