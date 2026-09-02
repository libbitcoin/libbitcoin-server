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
#ifndef LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_COIN_HPP
#define LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_COIN_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Utxo set coin serialization, the element of both set commitment forms.
BCS_API void to_coin_data(system::data_chunk& out,
    const database::unspent_coin& coin) NOEXCEPT;

/// Total subsidy issued through the given height (all coins in existence).
BCS_API uint64_t total_subsidy(const system::settings& settings,
    size_t height) NOEXCEPT;

/// The gettxoutsetinfo block_info object for the block at the given height.
BCS_API bool block_info(network::rpc::object_t& out, const node::query& query,
    const system::settings& settings, const database::header_link& link,
    size_t height) NOEXCEPT;

/// The scantxoutset result from the matched coins (canonical order).
BCS_API network::rpc::object_t scan_result(size_t& size,
    database::unspent_coins& coins, const node::query& query,
    const system::chain::scripts& scripts, size_t top, uint64_t txouts,
    bool bip30, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
