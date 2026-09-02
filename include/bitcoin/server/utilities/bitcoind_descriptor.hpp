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
#ifndef LIBBITCOIN_SERVER_UTILITIES_BITCOIND_DESCRIPTOR_HPP
#define LIBBITCOIN_SERVER_UTILITIES_BITCOIND_DESCRIPTOR_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The bip380 output descriptor checksum, empty on invalid characters.
BCS_API std::string descriptor_checksum(const std::string& descriptor) NOEXCEPT;

/// The output descriptor of a script, raw where no pattern is expressible.
BCS_API std::string infer_descriptor(const system::chain::script& script,
    uint8_t p2kh, uint8_t p2sh, const std::string& witness) NOEXCEPT;

/// The createmultisig result, empty if a key is invalid or the p2sh embedded
/// script exceeds one push element. An uncompressed key downgrades a segwit
/// address type to legacy with a warning (as bitcoind).
BCS_API network::rpc::object_t create_multisig(uint8_t required,
    const network::rpc::array_t& keys, const std::string& address_type,
    uint8_t p2sh, const std::string& witness) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
