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
#ifndef LIBBITCOIN_SERVER_UTILITIES_CHAIN_HPP
#define LIBBITCOIN_SERVER_UTILITIES_CHAIN_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The mediantime of the block (its window includes the block).
BCS_API uint32_t median_time(const node::query& query,
    const system::settings& settings,
    const database::header_link& link) NOEXCEPT;

/// Fee rate (satoshis per vbyte) and total vsize of each histogram bin.
using fee_histogram = std::vector<std::pair<double, size_t>>;

/// Histogram of descending fee rates, each bin exceeding the bin vsize
/// before a lower rate begins a new bin (electrs).
BCS_API fee_histogram to_fee_histogram(database::fee_rates rates,
    size_t bin) NOEXCEPT;

/// Fee histogram with bins growing by a tenth, partial omitted (electrumx).
BCS_API fee_histogram to_compact_fee_histogram(
    const database::fee_rates& rates, size_t bin) NOEXCEPT;

/// The confirmable txs count, vsize, total fee and fee histogram (esplora).
BCS_API bool get_pool_snapshot(system::json::value& out,
    const node::query& query) NOEXCEPT;

/// The confirmable txs as a bitcoind mempool.dat (version 1).
BCS_API bool serialize_mempool(system::data_chunk& out,
    const node::query& query) NOEXCEPT;

/// The txs of a bitcoind mempool.dat, deobfuscated in place (version 2).
BCS_API bool parse_mempool(system::chain::transaction_cptrs& out,
    system::data_chunk& data) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
