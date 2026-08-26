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
#ifndef LIBBITCOIN_SERVER_ERROR_BTCD_ERROR_T_HPP
#define LIBBITCOIN_SERVER_ERROR_BTCD_ERROR_T_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/server/error/error_t.hpp>

namespace libbitcoin {
namespace server {
namespace error {
namespace btcd {

/// Values are btcjson wire codes, published as the json-rpc error code.
/// unimplemented aliases misc_error, as btcd reports both as -1.
enum error_t : int32_t
{
    /// general
    success = 0,

    /// json-rpc
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_params = -32602,
    internal_error = -32603,
    parse_error = -32700,

    /// application
    misc_error = -1,
    unimplemented = -1,
    forbidden_by_safe_mode = -2,
    type_error = -3,
    wallet_error = -4,
    invalid_address_or_key = -5,
    wallet_insufficient_funds = -6,
    out_of_memory = -7,
    invalid_parameter = -8,
    client_not_connected = -9,
    client_in_initial_download = -10,
    wallet_invalid_account_name = -11,
    wallet_keypool_ran_out = -12,
    wallet_unlock_needed = -13,
    wallet_passphrase_incorrect = -14,
    wallet_wrong_enc_state = -15,
    wallet_encryption_failed = -16,
    wallet_already_unlocked = -17,
    wallet_not_found = -18,
    wallet_not_specified = -19,
    database_error = -20,
    deserialization_error = -22,
    client_node_already_added = -23,
    client_node_not_added = -24,
    verify_error = -25,
    verify_rejected = -26,
    verify_already_in_chain = -27,
    in_warmup = -28,
    client_node_not_connected = -29,
    client_invalid_ip_or_subnet = -30,
    method_deprecated = -32,
    client_mempool_disabled = -33
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

/// Map a foreign category code to the btcd code space, where a store fault
/// is internal and any other failure is reported as the given code.
/// A code of this category passes through unchanged.
BC_API code translate(const code& ec, error_t failure) NOEXCEPT;

} // namespace btcd
} // namespace error
} // namespace server
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::server::error::btcd::error)

#endif
