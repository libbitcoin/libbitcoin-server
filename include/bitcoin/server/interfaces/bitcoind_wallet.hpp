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
        method<"abandontransaction", string_t>{ unimplemented, "txid" },
        method<"abortrescan">{ unimplemented },
        method<"addhdkey", nullable<string_t>>{ unimplemented, "hdkey" },
        method<"backupwallet", string_t>{ unimplemented, "destination" },
        method<"bumpfee", string_t, nullable<object_t>>{ unimplemented, "txid", "options" },
        method<"createwallet", string_t, nullopt<false>, nullopt<false>, nullable<string_t>, nullopt<false>, nullopt<true>, nullable<boolean_t>, nullopt<false>>{ unimplemented, "wallet_name", "disable_private_keys", "blank", "passphrase", "avoid_reuse", "descriptors", "load_on_startup", "external_signer" },
        method<"createwalletdescriptor", string_t, nullable<object_t>>{ unimplemented, "type", "options" },
        method<"encryptwallet", string_t>{ unimplemented, "passphrase" },
        method<"exportwatchonlywallet", string_t>{ unimplemented, "destination" },
        method<"getaddressesbylabel", string_t>{ unimplemented, "label" },
        method<"getaddressinfo", string_t>{ unimplemented, "address" },
        method<"getbalance", nullable<string_t>, nullopt<0.0>, nullopt<false>, nullopt<true>>{ unimplemented, "dummy", "minconf", "include_watchonly", "avoid_reuse" },
        method<"getbalances">{ unimplemented },
        method<"gethdkeys", nullable<object_t>>{ unimplemented, "options" },
        method<"getnewaddress", nullopt<""_t>, nullable<string_t>>{ unimplemented, "label", "address_type" },
        method<"getrawchangeaddress", nullable<string_t>>{ unimplemented, "address_type" },
        method<"getreceivedbyaddress", string_t, nullopt<1.0>, nullopt<false>>{ unimplemented, "address", "minconf", "include_immature_coinbase" },
        method<"getreceivedbylabel", string_t, nullopt<1.0>, nullopt<false>>{ unimplemented, "label", "minconf", "include_immature_coinbase" },
        method<"gettransaction", string_t, nullopt<false>, nullopt<false>>{ unimplemented, "txid", "include_watchonly", "verbose" },
        method<"getwalletinfo">{ unimplemented },
        method<"importdescriptors", array_t>{ unimplemented, "requests" },
        method<"importprunedfunds", string_t, string_t>{ unimplemented, "rawtransaction", "txoutproof" },
        method<"keypoolrefill", nullable<number_t>>{ unimplemented, "newsize" },
        method<"listaddressgroupings">{ unimplemented },
        method<"listdescriptors", nullopt<false>>{ unimplemented, "private" },
        method<"listlabels", nullable<string_t>>{ unimplemented, "purpose" },
        method<"listlockunspent">{ unimplemented },
        method<"listreceivedbyaddress", nullopt<1.0>, nullopt<false>, nullopt<false>, nullable<string_t>, nullopt<false>>{ unimplemented, "minconf", "include_empty", "include_watchonly", "address_filter", "include_immature_coinbase" },
        method<"listreceivedbylabel", nullopt<1.0>, nullopt<false>, nullopt<false>, nullopt<false>>{ unimplemented, "minconf", "include_empty", "include_watchonly", "include_immature_coinbase" },
        method<"listsinceblock", nullable<string_t>, nullopt<1.0>, nullopt<false>, nullopt<true>, nullopt<false>, nullable<string_t>>{ unimplemented, "blockhash", "target_confirmations", "include_watchonly", "include_removed", "include_change", "label" },
        method<"listtransactions", nullable<string_t>, nullopt<10.0>, nullopt<0.0>, nullopt<false>>{ unimplemented, "label", "count", "skip", "include_watchonly" },
        method<"listunspent", nullopt<1.0>, nullopt<9999999.0>, nullopt<empty::array>, nullopt<true>, nullable<object_t>>{ unimplemented, "minconf", "maxconf", "addresses", "include_unsafe", "query_options" },
        method<"listwalletdir">{ unimplemented },
        method<"listwallets">{ unimplemented },
        method<"loadwallet", string_t, nullable<boolean_t>>{ unimplemented, "filename", "load_on_startup" },
        method<"lockunspent", boolean_t, nullopt<empty::array>, nullopt<false>>{ unimplemented, "unlock", "transactions", "persistent" },
        method<"migratewallet", nullable<string_t>, nullable<string_t>, nullopt<true>>{ unimplemented, "wallet_name", "passphrase", "load_wallet" },
        method<"psbtbumpfee", string_t, nullable<object_t>>{ unimplemented, "txid", "options" },
        method<"removeprunedfunds", string_t>{ unimplemented, "txid" },
        method<"rescanblockchain", nullopt<0.0>, nullable<number_t>>{ unimplemented, "start_height", "stop_height" },
        method<"restorewallet", string_t, string_t, nullable<boolean_t>>{ unimplemented, "wallet_name", "backup_file", "load_on_startup" },
        method<"send", value_t, nullable<number_t>, nullopt<"unset"_t>, nullable<number_t>, nullable<object_t>, nullopt<2.0>>{ unimplemented, "outputs", "conf_target", "estimate_mode", "fee_rate", "options", "version" },
        method<"sendall", array_t, nullable<number_t>, nullopt<"unset"_t>, nullable<number_t>, nullable<object_t>>{ unimplemented, "recipients", "conf_target", "estimate_mode", "fee_rate", "options" },
        method<"sendmany", nullopt<""_t>, nullopt<empty::object>, nullable<number_t>, nullable<string_t>, nullable<array_t>, nullable<boolean_t>, nullable<number_t>, nullopt<"unset"_t>, nullable<number_t>, nullopt<false>>{ unimplemented, "dummy", "amounts", "minconf", "comment", "subtractfeefrom", "replaceable", "conf_target", "estimate_mode", "fee_rate", "verbose" },
        method<"sendtoaddress", string_t, number_t, nullable<string_t>, nullable<string_t>, nullopt<false>, nullable<boolean_t>, nullable<number_t>, nullopt<"unset"_t>, nullopt<true>, nullable<number_t>, nullopt<false>>{ unimplemented, "address", "amount", "comment", "comment_to", "subtractfeefromamount", "replaceable", "conf_target", "estimate_mode", "avoid_reuse", "fee_rate", "verbose" },
        method<"setlabel", string_t, string_t>{ unimplemented, "address", "label" },
        method<"setwalletflag", string_t, nullopt<true>>{ unimplemented, "flag", "value" },
        method<"signmessage", string_t, string_t>{ unimplemented, "address", "message" },
        method<"signrawtransactionwithwallet", string_t, nullable<array_t>, nullable<string_t>>{ unimplemented, "hexstring", "prevtxs", "sighashtype" },
        method<"simulaterawtransaction", nullable<array_t>, nullable<object_t>>{ unimplemented, "rawtxs", "options" },
        method<"unloadwallet", nullable<string_t>, nullable<boolean_t>>{ unimplemented, "wallet_name", "load_on_startup" },
        method<"walletcreatefundedpsbt", nullable<array_t>, nullopt<empty::value>, nullopt<0.0>, nullable<object_t>, nullopt<true>, nullopt<2.0>, nullopt<2.0>>{ unimplemented, "inputs", "outputs", "locktime", "options", "bip32derivs", "version", "psbt_version" },
        method<"walletdisplayaddress", string_t>{ unimplemented, "address" },
        method<"walletlock">{ unimplemented },
        method<"walletpassphrase", string_t, number_t>{ unimplemented, "passphrase", "timeout" },
        method<"walletpassphrasechange", string_t, string_t>{ unimplemented, "oldpassphrase", "newpassphrase" },
        method<"walletprocesspsbt", string_t, nullopt<true>, nullable<string_t>, nullopt<true>, nullopt<true>>{ unimplemented, "psbt", "sign", "sighashtype", "bip32derivs", "finalize" }
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
