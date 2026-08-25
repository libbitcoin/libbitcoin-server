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
#ifndef LIBBITCOIN_SERVER_ERROR_HPP
#define LIBBITCOIN_SERVER_ERROR_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/server/version.hpp>

namespace libbitcoin {
namespace server {

/// Alias system code.
/// std::error_code "server" category holds server::error::error_t.
typedef std::error_code code;

namespace error {

/// Asio failures are normalized to the error codes below.
/// Stop by explicit call is mapped to channel_stopped or service_stopped
/// depending on the context. Asio errors returned on cancel calls are ignored.
enum error_t : uint8_t
{
    /// general
    success,

    /// server (url parse codes)
    empty_path,
    invalid_number,
    invalid_hash,
    missing_version,
    missing_target,
    invalid_target,
    missing_hash,
    missing_height,
    missing_position,
    missing_id_type,
    invalid_id_type,
    missing_type_id,
    missing_component,
    invalid_component,
    invalid_subcomponent,
    extra_segment,

    /// server (rpc response codes)
    not_found,
    not_implemented,
    invalid_argument,
    subscription_limit,
    unsupported_argument,
    unconfirmable_transaction,
    argument_overflow,
    target_overflow,
    maximum_depth,
    wrong_version,
    server_error,
    method_unauthorized
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

namespace bitcoind {

/// Values are bitcoind wire codes, published as the json-rpc error code.
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
    forbidden_by_safe_mode = -2,
    type_error = -3,
    invalid_address_or_key = -5,
    out_of_memory = -7,
    invalid_parameter = -8,
    database_error = -20,
    deserialization_error = -22,
    verify_error = -25,
    verify_rejected = -26,
    verify_already_in_utxo_set = -27,
    in_warmup = -28,
    method_deprecated = -32,
    limit_exceeded = -37,

    /// peer-to-peer client
    client_not_connected = -9,
    client_in_initial_download = -10,
    client_node_already_added = -23,
    client_node_not_added = -24,
    client_node_not_connected = -29,
    client_invalid_ip_or_subnet = -30,
    client_p2p_disabled = -31,
    client_node_capacity_reached = -34,

    /// chain
    client_mempool_disabled = -33,

    /// wallet
    wallet_error = -4,
    wallet_insufficient_funds = -6,
    wallet_invalid_label_name = -11,
    wallet_keypool_ran_out = -12,
    wallet_unlock_needed = -13,
    wallet_passphrase_incorrect = -14,
    wallet_wrong_enc_state = -15,
    wallet_encryption_failed = -16,
    wallet_already_unlocked = -17,
    wallet_not_found = -18,
    wallet_not_specified = -19,
    wallet_already_loaded = -35,
    wallet_already_exists = -36
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

} // namespace bitcoind
} // namespace error
} // namespace server
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::server::error::error)
DECLARE_STD_ERROR_REGISTRATION(bc::server::error::bitcoind::error)

#endif
