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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_WALLET_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_WALLET_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_wallet_methods
{
    static constexpr std::tuple methods
    {
        method<"enumeratesigners">{ unimplemented },
        method<"abandontransaction">{ unimplemented },
        method<"abortrescan">{ unimplemented },
        method<"addhdkey">{ unimplemented },
        method<"backupwallet">{ unimplemented },
        method<"bumpfee">{ unimplemented },
        method<"createwallet">{ unimplemented },
        method<"createwalletdescriptor">{ unimplemented },
        method<"encryptwallet">{ unimplemented },
        method<"exportwatchonlywallet">{ unimplemented },
        method<"getaddressesbylabel">{ unimplemented },
        method<"getaddressinfo">{ unimplemented },
        method<"getbalance">{ unimplemented },
        method<"getbalances">{ unimplemented },
        method<"gethdkeys">{ unimplemented },
        method<"getnewaddress">{ unimplemented },
        method<"getrawchangeaddress">{ unimplemented },
        method<"getreceivedbyaddress">{ unimplemented },
        method<"getreceivedbylabel">{ unimplemented },
        method<"gettransaction">{ unimplemented },
        method<"getwalletinfo">{ unimplemented },
        method<"importdescriptors">{ unimplemented },
        method<"importprunedfunds">{ unimplemented },
        method<"keypoolrefill">{ unimplemented },
        method<"listaddressgroupings">{ unimplemented },
        method<"listdescriptors">{ unimplemented },
        method<"listlabels">{ unimplemented },
        method<"listlockunspent">{ unimplemented },
        method<"listreceivedbyaddress">{ unimplemented },
        method<"listreceivedbylabel">{ unimplemented },
        method<"listsinceblock">{ unimplemented },
        method<"listtransactions">{ unimplemented },
        method<"listunspent">{ unimplemented },
        method<"listwalletdir">{ unimplemented },
        method<"listwallets">{ unimplemented },
        method<"loadwallet">{ unimplemented },
        method<"lockunspent">{ unimplemented },
        method<"migratewallet">{ unimplemented },
        method<"psbtbumpfee">{ unimplemented },
        method<"removeprunedfunds">{ unimplemented },
        method<"rescanblockchain">{ unimplemented },
        method<"restorewallet">{ unimplemented },
        method<"send">{ unimplemented },
        method<"sendall">{ unimplemented },
        method<"sendmany">{ unimplemented },
        method<"sendtoaddress">{ unimplemented },
        method<"setlabel">{ unimplemented },
        method<"setwalletflag">{ unimplemented },
        method<"signmessage">{ unimplemented },
        method<"signrawtransactionwithwallet">{ unimplemented },
        method<"simulaterawtransaction">{ unimplemented },
        method<"unloadwallet">{ unimplemented },
        method<"walletcreatefundedpsbt">{ unimplemented },
        method<"walletdisplayaddress">{ unimplemented },
        method<"walletlock">{ unimplemented },
        method<"walletpassphrase">{ unimplemented },
        method<"walletpassphrasechange">{ unimplemented },
        method<"walletprocesspsbt">{ unimplemented }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    /// Method names as reported by help.
    static constexpr auto name_data = method_names<methods>();
    static constexpr std::string_view names{ name_data.data(),
        name_data.size() };

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using enumerate_signers = at<0>;
    using abandon_transaction = at<1>;
    using abort_rescan = at<2>;
    using add_hd_key = at<3>;
    using backup_wallet = at<4>;
    using bump_fee = at<5>;
    using create_wallet = at<6>;
    using create_wallet_descriptor = at<7>;
    using encrypt_wallet = at<8>;
    using export_watchonly_wallet = at<9>;
    using get_addresses_by_label = at<10>;
    using get_address_info = at<11>;
    using get_balance = at<12>;
    using get_balances = at<13>;
    using get_hd_keys = at<14>;
    using get_new_address = at<15>;
    using get_raw_change_address = at<16>;
    using get_received_by_address = at<17>;
    using get_received_by_label = at<18>;
    using get_transaction = at<19>;
    using get_wallet_info = at<20>;
    using import_descriptors = at<21>;
    using import_pruned_funds = at<22>;
    using keypool_refill = at<23>;
    using list_address_groupings = at<24>;
    using list_descriptors = at<25>;
    using list_labels = at<26>;
    using list_lock_unspent = at<27>;
    using list_received_by_address = at<28>;
    using list_received_by_label = at<29>;
    using list_since_block = at<30>;
    using list_transactions = at<31>;
    using list_unspent = at<32>;
    using list_wallet_dir = at<33>;
    using list_wallets = at<34>;
    using load_wallet = at<35>;
    using lock_unspent = at<36>;
    using migrate_wallet = at<37>;
    using psbt_bump_fee = at<38>;
    using remove_pruned_funds = at<39>;
    using rescan_block_chain = at<40>;
    using restore_wallet = at<41>;
    using send = at<42>;
    using send_all = at<43>;
    using send_many = at<44>;
    using send_to_address = at<45>;
    using set_label = at<46>;
    using set_wallet_flag = at<47>;
    using sign_message = at<48>;
    using sign_raw_transaction_with_wallet = at<49>;
    using simulate_raw_transaction = at<50>;
    using unload_wallet = at<51>;
    using wallet_create_funded_psbt = at<52>;
    using wallet_display_address = at<53>;
    using wallet_lock = at<54>;
    using wallet_passphrase = at<55>;
    using wallet_passphrase_change = at<56>;
    using wallet_process_psbt = at<57>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
