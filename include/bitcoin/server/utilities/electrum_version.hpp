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

} // namespace electrum
} // namespace server
} // namespace libbitcoin

#endif
