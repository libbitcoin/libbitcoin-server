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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_WALLET_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_WALLET_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_wallet>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_wallet>;

class BCS_API protocol_bitcoind_wallet
  : public protocol_bitcoind_dispatch<interface::bitcoind_wallet>,
    protected network::tracker<protocol_bitcoind_wallet>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_wallet> ptr;
    using rpc_interface = interface::bitcoind_wallet;

    inline protocol_bitcoind_wallet(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<interface::bitcoind_wallet>(session,
            channel, options),
        network::tracker<protocol_bitcoind_wallet>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_enumerate_signers(const code& ec,
        rpc_interface::enumerate_signers) NOEXCEPT;
    bool handle_abandon_transaction(const code& ec,
        rpc_interface::abandon_transaction) NOEXCEPT;
    bool handle_abort_rescan(const code& ec,
        rpc_interface::abort_rescan) NOEXCEPT;
    bool handle_add_hd_key(const code& ec,
        rpc_interface::add_hd_key) NOEXCEPT;
    bool handle_backup_wallet(const code& ec,
        rpc_interface::backup_wallet) NOEXCEPT;
    bool handle_bump_fee(const code& ec,
        rpc_interface::bump_fee) NOEXCEPT;
    bool handle_create_wallet(const code& ec,
        rpc_interface::create_wallet) NOEXCEPT;
    bool handle_create_wallet_descriptor(const code& ec,
        rpc_interface::create_wallet_descriptor) NOEXCEPT;
    bool handle_encrypt_wallet(const code& ec,
        rpc_interface::encrypt_wallet) NOEXCEPT;
    bool handle_export_watchonly_wallet(const code& ec,
        rpc_interface::export_watchonly_wallet) NOEXCEPT;
    bool handle_get_addresses_by_label(const code& ec,
        rpc_interface::get_addresses_by_label) NOEXCEPT;
    bool handle_get_address_info(const code& ec,
        rpc_interface::get_address_info) NOEXCEPT;
    bool handle_get_balance(const code& ec,
        rpc_interface::get_balance) NOEXCEPT;
    bool handle_get_balances(const code& ec,
        rpc_interface::get_balances) NOEXCEPT;
    bool handle_get_hd_keys(const code& ec,
        rpc_interface::get_hd_keys) NOEXCEPT;
    bool handle_get_new_address(const code& ec,
        rpc_interface::get_new_address) NOEXCEPT;
    bool handle_get_raw_change_address(const code& ec,
        rpc_interface::get_raw_change_address) NOEXCEPT;
    bool handle_get_received_by_address(const code& ec,
        rpc_interface::get_received_by_address) NOEXCEPT;
    bool handle_get_received_by_label(const code& ec,
        rpc_interface::get_received_by_label) NOEXCEPT;
    bool handle_get_transaction(const code& ec,
        rpc_interface::get_transaction) NOEXCEPT;
    bool handle_get_wallet_info(const code& ec,
        rpc_interface::get_wallet_info) NOEXCEPT;
    bool handle_import_descriptors(const code& ec,
        rpc_interface::import_descriptors) NOEXCEPT;
    bool handle_import_pruned_funds(const code& ec,
        rpc_interface::import_pruned_funds) NOEXCEPT;
    bool handle_keypool_refill(const code& ec,
        rpc_interface::keypool_refill) NOEXCEPT;
    bool handle_list_address_groupings(const code& ec,
        rpc_interface::list_address_groupings) NOEXCEPT;
    bool handle_list_descriptors(const code& ec,
        rpc_interface::list_descriptors) NOEXCEPT;
    bool handle_list_labels(const code& ec,
        rpc_interface::list_labels) NOEXCEPT;
    bool handle_list_lock_unspent(const code& ec,
        rpc_interface::list_lock_unspent) NOEXCEPT;
    bool handle_list_received_by_address(const code& ec,
        rpc_interface::list_received_by_address) NOEXCEPT;
    bool handle_list_received_by_label(const code& ec,
        rpc_interface::list_received_by_label) NOEXCEPT;
    bool handle_list_since_block(const code& ec,
        rpc_interface::list_since_block) NOEXCEPT;
    bool handle_list_transactions(const code& ec,
        rpc_interface::list_transactions) NOEXCEPT;
    bool handle_list_unspent(const code& ec,
        rpc_interface::list_unspent) NOEXCEPT;
    bool handle_list_wallet_dir(const code& ec,
        rpc_interface::list_wallet_dir) NOEXCEPT;
    bool handle_list_wallets(const code& ec,
        rpc_interface::list_wallets) NOEXCEPT;
    bool handle_load_wallet(const code& ec,
        rpc_interface::load_wallet) NOEXCEPT;
    bool handle_lock_unspent(const code& ec,
        rpc_interface::lock_unspent) NOEXCEPT;
    bool handle_migrate_wallet(const code& ec,
        rpc_interface::migrate_wallet) NOEXCEPT;
    bool handle_psbt_bump_fee(const code& ec,
        rpc_interface::psbt_bump_fee) NOEXCEPT;
    bool handle_remove_pruned_funds(const code& ec,
        rpc_interface::remove_pruned_funds) NOEXCEPT;
    bool handle_rescan_block_chain(const code& ec,
        rpc_interface::rescan_block_chain) NOEXCEPT;
    bool handle_restore_wallet(const code& ec,
        rpc_interface::restore_wallet) NOEXCEPT;
    bool handle_wallet_send(const code& ec,
        rpc_interface::send) NOEXCEPT;
    bool handle_send_all(const code& ec,
        rpc_interface::send_all) NOEXCEPT;
    bool handle_send_many(const code& ec,
        rpc_interface::send_many) NOEXCEPT;
    bool handle_send_to_address(const code& ec,
        rpc_interface::send_to_address) NOEXCEPT;
    bool handle_set_label(const code& ec,
        rpc_interface::set_label) NOEXCEPT;
    bool handle_set_wallet_flag(const code& ec,
        rpc_interface::set_wallet_flag) NOEXCEPT;
    bool handle_sign_message(const code& ec,
        rpc_interface::sign_message) NOEXCEPT;
    bool handle_sign_raw_transaction_with_wallet(const code& ec,
        rpc_interface::sign_raw_transaction_with_wallet) NOEXCEPT;
    bool handle_simulate_raw_transaction(const code& ec,
        rpc_interface::simulate_raw_transaction) NOEXCEPT;
    bool handle_unload_wallet(const code& ec,
        rpc_interface::unload_wallet) NOEXCEPT;
    bool handle_wallet_create_funded_psbt(const code& ec,
        rpc_interface::wallet_create_funded_psbt) NOEXCEPT;
    bool handle_wallet_display_address(const code& ec,
        rpc_interface::wallet_display_address) NOEXCEPT;
    bool handle_wallet_lock(const code& ec,
        rpc_interface::wallet_lock) NOEXCEPT;
    bool handle_wallet_passphrase(const code& ec,
        rpc_interface::wallet_passphrase) NOEXCEPT;
    bool handle_wallet_passphrase_change(const code& ec,
        rpc_interface::wallet_passphrase_change) NOEXCEPT;
    bool handle_wallet_process_psbt(const code& ec,
        rpc_interface::wallet_process_psbt) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
