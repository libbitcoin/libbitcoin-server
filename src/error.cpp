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
#include <bitcoin/server/error.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace error {

DEFINE_ERROR_T_MESSAGE_MAP(error)
{
    // general
    { success, "success" },

    // server (url parse codes)
    { empty_path, "empty_path" },
    { invalid_number, "invalid_number" },
    { invalid_hash, "invalid_hash" },
    { missing_version, "missing_version" },
    { missing_target, "missing_target" },
    { invalid_target, "invalid_target" },
    { missing_hash, "missing_hash" },
    { missing_height, "missing_height" },
    { missing_position, "missing_position" },
    { missing_id_type, "missing_id_type" },
    { invalid_id_type, "invalid_id_type" },
    { missing_type_id, "missing_type_id" },
    { missing_component, "missing_component" },
    { invalid_component, "invalid_component" },
    { invalid_subcomponent, "invalid_subcomponent" },
    { extra_segment, "extra_segment" },

    // server (rpc response codes)
    { not_found, "not_found" },
    { not_implemented, "not_implemented" },
    { invalid_argument, "invalid_argument" },
    { subscription_limit, "subscription_limit" },
    { unsupported_argument, "unsupported_argument" },
    { unconfirmable_transaction, "unconfirmable_transaction" },
    { argument_overflow, "argument_overflow" },
    { target_overflow, "target_overflow" },
    { maximum_depth, "maximum_depth" },
    { wrong_version, "wrong_version" },
    { server_error, "server_error" },
    { method_unauthorized, "method_unauthorized" }
};

DEFINE_ERROR_T_CATEGORY(error, "server", "server code")

namespace bitcoind {

DEFINE_ERROR_T_MESSAGE_MAP(error)
{
    // general
    { success, "success" },

    // json-rpc
    { invalid_request, "invalid_request" },
    { method_not_found, "method_not_found" },
    { invalid_params, "invalid_params" },
    { internal_error, "internal_error" },
    { parse_error, "parse_error" },

    // application
    { misc_error, "misc_error" },
    { forbidden_by_safe_mode, "forbidden_by_safe_mode" },
    { type_error, "type_error" },
    { invalid_address_or_key, "invalid_address_or_key" },
    { out_of_memory, "out_of_memory" },
    { invalid_parameter, "invalid_parameter" },
    { database_error, "database_error" },
    { deserialization_error, "deserialization_error" },
    { verify_error, "verify_error" },
    { verify_rejected, "verify_rejected" },
    { verify_already_in_utxo_set, "verify_already_in_utxo_set" },
    { in_warmup, "in_warmup" },
    { method_deprecated, "method_deprecated" },
    { limit_exceeded, "limit_exceeded" },

    // peer-to-peer client
    { client_not_connected, "client_not_connected" },
    { client_in_initial_download, "client_in_initial_download" },
    { client_node_already_added, "client_node_already_added" },
    { client_node_not_added, "client_node_not_added" },
    { client_node_not_connected, "client_node_not_connected" },
    { client_invalid_ip_or_subnet, "client_invalid_ip_or_subnet" },
    { client_p2p_disabled, "client_p2p_disabled" },
    { client_node_capacity_reached, "client_node_capacity_reached" },

    // chain
    { client_mempool_disabled, "client_mempool_disabled" },

    // wallet
    { wallet_error, "wallet_error" },
    { wallet_insufficient_funds, "wallet_insufficient_funds" },
    { wallet_invalid_label_name, "wallet_invalid_label_name" },
    { wallet_keypool_ran_out, "wallet_keypool_ran_out" },
    { wallet_unlock_needed, "wallet_unlock_needed" },
    { wallet_passphrase_incorrect, "wallet_passphrase_incorrect" },
    { wallet_wrong_enc_state, "wallet_wrong_enc_state" },
    { wallet_encryption_failed, "wallet_encryption_failed" },
    { wallet_already_unlocked, "wallet_already_unlocked" },
    { wallet_not_found, "wallet_not_found" },
    { wallet_not_specified, "wallet_not_specified" },
    { wallet_already_loaded, "wallet_already_loaded" },
    { wallet_already_exists, "wallet_already_exists" }
};

DEFINE_ERROR_T_CATEGORY(error, "bitcoind", "bitcoind code")

} // namespace bitcoind
} // namespace error
} // namespace server
} // namespace libbitcoin
