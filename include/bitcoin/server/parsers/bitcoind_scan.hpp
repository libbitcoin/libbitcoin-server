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
#ifndef LIBBITCOIN_SERVER_PARSERS_BITCOIND_SCAN_HPP
#define LIBBITCOIN_SERVER_PARSERS_BITCOIND_SCAN_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Expand a scan object, a descriptor string or { "desc", "range" } object,
/// to its derived output scripts (false if malformed or underivable).
BCS_API bool expand_scan_object(system::chain::scripts& out,
    const network::rpc::value_t& item) NOEXCEPT;

/// As expand_scan_object, retaining embedded scripts and key origins.
BCS_API bool expand_scan_signings(
    system::wallet::descriptor::signing::list& out,
    const network::rpc::value_t& item) NOEXCEPT;

/// Parse a derivation range, an end index or a [begin, end] pair.
BCS_API bool parse_scan_range(uint32_t& begin, uint32_t& end,
    const network::rpc::value_t& range) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
