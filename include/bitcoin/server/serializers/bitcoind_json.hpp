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
#ifndef LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_JSON_HPP
#define LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_JSON_HPP

#include <string>
#include <unordered_set>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Clamped ratio of validated blocks to chain height.
BCS_API double progress(size_t blocks, size_t headers) NOEXCEPT;

/// bitcoind's mediantime of the block (its window includes the block).
BCS_API uint32_t median_time(const node::query& query,
    const system::settings& settings,
    const database::header_link& link) NOEXCEPT;

/// A getchainstates entry for candidate or confirmed at the link (top).
BCS_API network::rpc::object_t chain_states_entry(const node::query& query,
    const database::header_link& link, double progress,
    bool validated) NOEXCEPT;

/// Block confirmation context (height, confirmations, siblings).
BCS_API void inject_block_context(boost::json::object& out,
    const node::query& query, const system::settings& settings,
    const database::header_link& link,
    const system::chain::header& header) NOEXCEPT;

/// Transaction confirmation context (blockhash, confirmations, time).
BCS_API void inject_tx_context(boost::json::object& out,
    const node::query& query, const database::tx_link& link) NOEXCEPT;

/// Per-input prevout context (requires populate_with_metadata).
BCS_API void inject_tx_prevouts(boost::json::object& out,
    const node::query& query,
    const system::chain::transaction& tx) NOEXCEPT;

/// The address of a singular output script (empty if unaddressable).
BCS_API std::string to_address(const system::chain::script& script,
    uint8_t p2kh, uint8_t p2sh, const std::string& witness) NOEXCEPT;

/// Append the block's receive/spend activity on the watched scripts (hex
/// encoded), requires populate_without_metadata (getdescriptoractivity).
BCS_API void inject_activity(network::rpc::array_t& out,
    const system::chain::block& block, size_t height,
    const std::string& blockhash,
    const std::unordered_set<std::string>& watch) NOEXCEPT;

/// The bitcoind block header object.
BCS_API boost::json::object header_to_bitcoind(
    const system::chain::header& header) NOEXCEPT;

/// The bitcoind chain name, resolved from the genesis block.
BCS_API std::string chain_name(const node::query& query) NOEXCEPT;

/// The getblockchaininfo result, bitcoind field set (btcd augments it).
/// False if the store is inconsistent, the caller sends the error.
BCS_API bool chain_info(network::rpc::object_t& out,
    const node::query& query, const system::settings& settings,
    bool pruned, bool current) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
