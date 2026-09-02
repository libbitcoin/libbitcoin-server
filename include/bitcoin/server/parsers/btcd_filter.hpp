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
#ifndef LIBBITCOIN_SERVER_PARSERS_BTCD_FILTER_HPP
#define LIBBITCOIN_SERVER_PARSERS_BTCD_FILTER_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace btcd {

/// Parse loadtxfilter addresses to their output script hashes.
BCS_API code filter_keys(system::hashes& out,
    const network::rpc::value_t& addresses, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness) NOEXCEPT;

/// Parse loadtxfilter outpoints to their points.
BCS_API code filter_points(system::chain::points& out,
    const network::rpc::value_t& outpoints) NOEXCEPT;

/// Parse a searchrawtransactions address to its output script hashes. A
/// serialized public key implies two scripts (p2pk and its derived p2pkh).
BCS_API code search_keys(system::hashes& out, const std::string& address,
    uint8_t p2kh, uint8_t p2sh, const std::string& witness) NOEXCEPT;

} // namespace btcd
} // namespace server
} // namespace libbitcoin

#endif
