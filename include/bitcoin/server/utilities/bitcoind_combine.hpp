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
#ifndef LIBBITCOIN_SERVER_UTILITIES_BITCOIND_COMBINE_HPP
#define LIBBITCOIN_SERVER_UTILITIES_BITCOIND_COMBINE_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The combined input from the variants' candidates at the same position.
/// Multisig (bare, p2sh, p2wsh) merges endorsements across the candidates;
/// any other form is taken from the first candidate carrying a signature.
BCS_API code combine_input(system::chain::input::cptr& out,
    const node::query& query, const system::chain::transaction_cptrs& variants,
    uint32_t index) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
