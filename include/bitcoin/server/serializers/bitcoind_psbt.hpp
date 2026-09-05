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
#ifndef LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_PSBT_HPP
#define LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_PSBT_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The bip32 key path string ("m/0'/1") of a derivation path.
BCS_API std::string to_key_path(const std::vector<uint32_t>& path) NOEXCEPT;

/// The proprietary (unknown) entries of a psbt map.
BCS_API network::rpc::object_t to_unknown(
    const system::wallet::psbt::entry::list& entries) NOEXCEPT;

/// The bitcoind decodepsbt input object.
BCS_API network::rpc::object_t decode_psbt_input(
    const system::wallet::psbt::input& in, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness, uint32_t flags) NOEXCEPT;

/// The bitcoind decodepsbt output object.
BCS_API network::rpc::object_t decode_psbt_output(
    const system::wallet::psbt::output& out, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness, uint32_t flags) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
