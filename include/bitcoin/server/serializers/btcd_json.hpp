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
#ifndef LIBBITCOIN_SERVER_SERIALIZERS_BTCD_JSON_HPP
#define LIBBITCOIN_SERVER_SERIALIZERS_BTCD_JSON_HPP

#include <set>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace btcd {

/// The verbose searchrawtransactions element: the bitcoind transaction model
/// with btcd's prevOut extension and address filtering.
BCS_API boost::json::object search_transaction(const node::query& query,
    const database::tx_link& link, const system::chain::transaction& tx,
    const std::set<std::string>& filter, bool prevouts, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT;

} // namespace btcd
} // namespace server
} // namespace libbitcoin

#endif
