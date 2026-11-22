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

/// Add btcd's reqSigs, address and addresses to an output script model.
BCS_API void inject_script(system::json::object& out,
    const system::chain::script& script, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness) NOEXCEPT;

/// The btcd output script model (no descriptor).
BCS_API system::json::value script_public_key(
    const system::chain::script& script, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness, uint32_t flags) NOEXCEPT;

/// Add btcd script fields to the outputs of a bitcoind transaction model,
/// and disassemble its input scripts without sighash types.
BCS_API void inject_scripts(system::json::object& out,
    const system::chain::transaction& tx, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness, uint32_t flags) NOEXCEPT;

/// Add the btcd block context of a transaction (no in_active_chain, and no
/// confirmations when unconfirmed).
BCS_API void inject_tx_context(system::json::object& out,
    const node::query& query, const database::tx_link& link) NOEXCEPT;

/// The btcd verbose transaction, with btcd's prevOut extension and address
/// filtering.
BCS_API system::json::object verbose_transaction(const node::query& query,
    const database::tx_link& link, const system::chain::transaction& tx,
    const std::set<std::string>& filter, bool prevouts, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT;

/// The verbose searchrawtransactions element, with empty hash and sizes.
BCS_API system::json::object search_transaction(const node::query& query,
    const database::tx_link& link, const system::chain::transaction& tx,
    const std::set<std::string>& filter, bool prevouts, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT;

} // namespace btcd
} // namespace server
} // namespace libbitcoin

#endif
