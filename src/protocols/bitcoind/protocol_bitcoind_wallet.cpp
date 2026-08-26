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
#include <bitcoin/server/protocols/protocol_bitcoind_wallet.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_wallet
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_wallet::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_enumerate_signers, _1, _2);
    SUBSCRIBE_BITCOIND(handle_abandon_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_abort_rescan, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_hd_key, _1, _2);
    SUBSCRIBE_BITCOIND(handle_backup_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_bump_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_wallet_descriptor, _1, _2);
    SUBSCRIBE_BITCOIND(handle_encrypt_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_export_watchonly_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_addresses_by_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_address_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_balance, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_balances, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_hd_keys, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_new_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_change_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_received_by_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_received_by_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_wallet_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_descriptors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_pruned_funds, _1, _2);
    SUBSCRIBE_BITCOIND(handle_keypool_refill, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_address_groupings, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_descriptors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_labels, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_lock_unspent, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_received_by_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_received_by_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_since_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_transactions, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_unspent, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_wallet_dir, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_wallets, _1, _2);
    SUBSCRIBE_BITCOIND(handle_load_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_lock_unspent, _1, _2);
    SUBSCRIBE_BITCOIND(handle_migrate_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_psbt_bump_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_remove_pruned_funds, _1, _2);
    SUBSCRIBE_BITCOIND(handle_rescan_block_chain, _1, _2);
    SUBSCRIBE_BITCOIND(handle_restore_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_send, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_all, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_many, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_to_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_wallet_flag, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sign_message, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sign_raw_transaction_with_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_simulate_raw_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_unload_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_create_funded_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_display_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_lock, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_passphrase, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_passphrase_change, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_process_psbt, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Wallet methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_wallet::handle_abandon_transaction(const code& ec,
    rpc_interface::abandon_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_abort_rescan(const code& ec,
    rpc_interface::abort_rescan) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_add_hd_key(const code& ec,
    rpc_interface::add_hd_key) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_backup_wallet(const code& ec,
    rpc_interface::backup_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_bump_fee(const code& ec,
    rpc_interface::bump_fee) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_create_wallet(const code& ec,
    rpc_interface::create_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_create_wallet_descriptor(const code& ec,
    rpc_interface::create_wallet_descriptor) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_encrypt_wallet(const code& ec,
    rpc_interface::encrypt_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_export_watchonly_wallet(const code& ec,
    rpc_interface::export_watchonly_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_addresses_by_label(const code& ec,
    rpc_interface::get_addresses_by_label) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_address_info(const code& ec,
    rpc_interface::get_address_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_balance(const code& ec,
    rpc_interface::get_balance) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_balances(const code& ec,
    rpc_interface::get_balances) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_hd_keys(const code& ec,
    rpc_interface::get_hd_keys) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_new_address(const code& ec,
    rpc_interface::get_new_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_raw_change_address(const code& ec,
    rpc_interface::get_raw_change_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_received_by_address(const code& ec,
    rpc_interface::get_received_by_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_received_by_label(const code& ec,
    rpc_interface::get_received_by_label) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_transaction(const code& ec,
    rpc_interface::get_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_get_wallet_info(const code& ec,
    rpc_interface::get_wallet_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_import_descriptors(const code& ec,
    rpc_interface::import_descriptors) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_import_pruned_funds(const code& ec,
    rpc_interface::import_pruned_funds) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_keypool_refill(const code& ec,
    rpc_interface::keypool_refill) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_address_groupings(const code& ec,
    rpc_interface::list_address_groupings) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_descriptors(const code& ec,
    rpc_interface::list_descriptors) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_labels(const code& ec,
    rpc_interface::list_labels) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_lock_unspent(const code& ec,
    rpc_interface::list_lock_unspent) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_received_by_address(const code& ec,
    rpc_interface::list_received_by_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_received_by_label(const code& ec,
    rpc_interface::list_received_by_label) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_since_block(const code& ec,
    rpc_interface::list_since_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_transactions(const code& ec,
    rpc_interface::list_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_unspent(const code& ec,
    rpc_interface::list_unspent) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_wallet_dir(const code& ec,
    rpc_interface::list_wallet_dir) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_list_wallets(const code& ec,
    rpc_interface::list_wallets) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_load_wallet(const code& ec,
    rpc_interface::load_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_lock_unspent(const code& ec,
    rpc_interface::lock_unspent) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_migrate_wallet(const code& ec,
    rpc_interface::migrate_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_psbt_bump_fee(const code& ec,
    rpc_interface::psbt_bump_fee) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_remove_pruned_funds(const code& ec,
    rpc_interface::remove_pruned_funds) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_rescan_block_chain(const code& ec,
    rpc_interface::rescan_block_chain) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_restore_wallet(const code& ec,
    rpc_interface::restore_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_send(const code& ec,
    rpc_interface::send) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_send_all(const code& ec,
    rpc_interface::send_all) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_send_many(const code& ec,
    rpc_interface::send_many) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_send_to_address(const code& ec,
    rpc_interface::send_to_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_set_label(const code& ec,
    rpc_interface::set_label) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_set_wallet_flag(const code& ec,
    rpc_interface::set_wallet_flag) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_sign_message(const code& ec,
    rpc_interface::sign_message) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_sign_raw_transaction_with_wallet(const code& ec,
    rpc_interface::sign_raw_transaction_with_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_simulate_raw_transaction(const code& ec,
    rpc_interface::simulate_raw_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_unload_wallet(const code& ec,
    rpc_interface::unload_wallet) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_create_funded_psbt(const code& ec,
    rpc_interface::wallet_create_funded_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_display_address(const code& ec,
    rpc_interface::wallet_display_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_lock(const code& ec,
    rpc_interface::wallet_lock) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_passphrase(const code& ec,
    rpc_interface::wallet_passphrase) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_passphrase_change(const code& ec,
    rpc_interface::wallet_passphrase_change) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_wallet_process_psbt(const code& ec,
    rpc_interface::wallet_process_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_wallet::handle_enumerate_signers(const code& ec,
    rpc_interface::enumerate_signers) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
