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
#include <bitcoin/server/error/bitcoind_error_t.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace error {
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

code translate(const code& ec, error_t failure) NOEXCEPT
{
    if (!ec)
        return success;

    if (error_category::contains(ec))
        return ec;

    if (database::error::error_category::contains(ec))
        return internal_error;

    return failure;
}

// bitcoind reject tokens by transaction code (mempool vocabulary).
static const message_map<system::error::transaction_error_t>
transaction_rejects
{
    { system::error::empty_transaction, "bad-txns-vin-empty" },
    { system::error::previous_output_null, "bad-txns-prevout-null" },
    { system::error::spend_overflow, "bad-txns-txouttotal-toolarge" },
    { system::error::invalid_coinbase_script_size, "bad-cb-length" },
    { system::error::coinbase_transaction, "coinbase" },
    { system::error::transaction_internal_double_spend, "bad-txns-inputs-duplicate" },
    { system::error::transaction_size_limit, "bad-txns-oversize" },
    { system::error::transaction_legacy_sigop_limit, "bad-txns-too-many-sigops" },
    { system::error::unspent_duplicate, "bad-txns-BIP30" },
    { system::error::missing_previous_output, "bad-txns-inputs-missingorspent" },
    { system::error::coinbase_maturity, "bad-txns-premature-spend-of-coinbase" },
    { system::error::spend_exceeds_value, "bad-txns-in-belowout" },
    { system::error::transaction_sigop_limit, "bad-txns-too-many-sigops" },
    { system::error::absolute_time_locked, "non-final" },
    { system::error::relative_time_locked, "non-BIP68-final" },
    { system::error::transaction_weight_limit, "tx-size" },
    { system::error::confirmed_double_spend, "bad-txns-inputs-missingorspent" }
};

// bitcoind reject tokens by block code (submission vocabulary).
static const message_map<system::error::block_error_t>
block_rejects
{
    { system::error::invalid_proof_of_work, "high-hash" },
    { system::error::futuristic_timestamp, "time-too-new" },
    { system::error::insufficient_block_version, "bad-version" },
    { system::error::anachronistic_timestamp, "time-too-old" },
    { system::error::incorrect_proof_of_work, "bad-diffbits" },
    { system::error::early_timestamp, "time-too-old" },
    { system::error::orphan_block, "prev-blk-not-found" },
    { system::error::block_size_limit, "bad-blk-length" },
    { system::error::empty_block, "bad-blk-length" },
    { system::error::first_not_coinbase, "bad-cb-missing" },
    { system::error::extra_coinbases, "bad-cb-multiple" },
    { system::error::internal_duplicate, "bad-txns-duplicate" },
    { system::error::block_internal_double_spend, "bad-txns-inputs-missingorspent" },
    { system::error::forward_reference, "bad-txns-inputs-missingorspent" },
    { system::error::invalid_transaction_commitment, "bad-txnmrklroot" },
    { system::error::block_legacy_sigop_limit, "bad-blk-sigops" },
    { system::error::block_non_final, "bad-txns-nonfinal" },
    { system::error::coinbase_height_mismatch, "bad-cb-height" },
    { system::error::coinbase_value_limit, "bad-cb-amount" },
    { system::error::block_sigop_limit, "bad-blk-sigops" },
    { system::error::invalid_witness_commitment, "bad-witness-merkle-match" },
    { system::error::block_weight_limit, "bad-blk-weight" },
    { system::error::invalid_signature, "mandatory-script-verify-flag-failed" },
    { system::error::unspent_coinbase_collision, "bad-txns-BIP30" }
};

std::string reject(const code& ec) NOEXCEPT
{
    // The organizer reports an unassociated block as a node orphan.
    if ((ec == node::error::orphan_block) || (ec == node::error::orphan_header))
        return "prev-blk-not-found";

    if ((ec == node::error::duplicate_block) ||
        (ec == node::error::duplicate_header))
        return "duplicate";

    using namespace system::error;
    if (transaction_error_category::contains(ec))
    {
        const auto token = transaction_rejects.find(
            static_cast<transaction_error_t>(ec.value()));
        if (token != transaction_rejects.end())
            return token->second;
    }

    if (block_error_category::contains(ec))
    {
        const auto token = block_rejects.find(
            static_cast<block_error_t>(ec.value()));
        if (token != block_rejects.end())
            return token->second;
    }

    return ec.message();
}

} // namespace bitcoind
} // namespace error
} // namespace server
} // namespace libbitcoin
