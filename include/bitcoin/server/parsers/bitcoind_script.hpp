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
#ifndef LIBBITCOIN_SERVER_PARSERS_BITCOIND_SCRIPT_HPP
#define LIBBITCOIN_SERVER_PARSERS_BITCOIND_SCRIPT_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

// TODO: move into bitcoind namespace (collides with json wrapper).

/// Parse an address to its output script, prefixes are validated.
BCS_API code output_script(system::chain::script& out,
    const std::string& text, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
