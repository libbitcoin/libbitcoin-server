/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_RPC_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_RPC_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_rpc_methods
{
    static constexpr std::tuple methods
    {
        /// Blockchain methods.
        method<"getbestblockhash">{},
        method<"getblock", string_t, optional<1.0>>{ "blockhash", "verbosity" },
        method<"getblockchaininfo">{},
        method<"getblockcount">{},
        method<"getblockfilter", string_t, optional<"basic"_t>>{ "blockhash", "filtertype" },
        method<"getblockhash", number_t>{ "height" },
        method<"getblockheader", string_t, optional<true>>{ "blockhash", "verbose" },
        method<"getblockstats", string_t, optional<empty::array>>{ "hash_or_height", "stats" },
        method<"getchaintxstats", optional<-1.0>, optional<""_t>>{ "nblocks", "blockhash" },
        method<"getchainwork">{},
        method<"gettxout", string_t, number_t, optional<true>>{ "txid", "n", "include_mempool" },
        method<"gettxoutsetinfo">{},
        method<"pruneblockchain", number_t>{ "height" },
        method<"savemempool">{},
        method<"scantxoutset", string_t, optional<empty::array>>{ "action", "scanobjects" },
        method<"verifychain", optional<4.0>, optional<288.0>>{ "checklevel", "nblocks" },
        method<"verifytxoutset", string_t>{ "input_verify_flag" },

        /////// Control methods.
        ////method<"getmemoryinfo", optional<"stats"_t>>{ "mode" },
        ////method<"getrpcinfo">{},
        ////method<"help", optional<""_t>>{ "command" },
        ////method<"logging", optional<"*"_t>>{ "include" },
        ////method<"stop">{},
        ////method<"uptime">{},

        /////// Mining methods.
        ////method<"getblocktemplate", optional<empty::object>>{ "template_request" },
        ////method<"getmininginfo">{},
        ////method<"getnetworkhashps", optional<120_u32>, optional<-1_i32>>{ "nblocks", "height" },
        ////method<"prioritisetransaction", string_t, number_t, number_t>{ "txid", "dummy", "priority_delta" },
        ////method<"submitblock", string_t, optional<""_t>>{ "block", "parameters" },

        /////// Network methods.
        ////method<"addnode", string_t, string_t>{ "node", "command" },
        ////method<"clearbanned">{},
        ////method<"disconnectnode", string_t, optional<-1_i32>>{ "address", "nodeid" },
        ////method<"getaddednodeinfo", optional<false>, optional<true>, optional<""_t>>{ "include_chain_info", "dns", "addnode" },
        ////method<"getconnectioncount">{},
        method<"getnetworkinfo">{}
        ////method<"getpeerinfo">{},
        ////method<"listbanned">{},
        ////method<"ping">{},
        ////method<"setban", string_t, string_t, optional<86400_u32>, optional<false>, optional<""_t>>{ "addr", "command", "bantime", "absolute", "reason" },
        ////method<"setnetworkactive", boolean_t>{ "state" },

        /////// Rawtransactions methods.
        ////method<"combinerawtransaction", array_t>{ "txs" },
        ////method<"createrawtransaction", array_t, object_t, optional<0_u32>, optional<false>>{ "inputs", "outputs", "locktime", "replaceable" },
        ////method<"decoderawtransaction", string_t>{ "hexstring" },
        ////method<"fundrawtransaction", string_t, optional<empty::object>>{ "rawtx", "options" },
        ////method<"getrawtransaction", string_t, optional<0_u32>, optional<""_t>>{ "txid", "verbose", "blockhash" },
        ////method<"sendrawtransaction", string_t, optional<0_u32>>{ "hexstring", "maxfeerate" },
        ////method<"signrawtransactionwithkey", string_t, optional<empty::array>, optional<empty::array>, optional<"ALL|FORKID"_t>>{ "hexstring", "privkeys", "prevtxs", "sighashtype" },
        ////method<"testmempoolaccept", array_t, optional<0_u32>>{ "rawtxs", "maxfeerate" },
        ////method<"testrawtransaction", string_t>{ "rawtx" },

        /////// Util methods (node-related).
        ////method<"createmultisig", number_t, array_t>{ "nrequired", "keys" },
        ////method<"decodepsbt", string_t>{ "psbt" },
        ////method<"decodescript", string_t>{ "hex" },
        ////method<"estimaterawfee", number_t, optional<"unset"_t>>{ "conf_target", "estimate_mode" },
        ////method<"getdescriptorinfo", string_t>{ "descriptor" },
        ////method<"validateaddress", string_t>{ "address" },

        /////// Wallet methods (unsupported).
        ////method<"abandontransaction", string_t>{ "txid" },
        ////method<"addmultisigaddress", number_t, array_t, optional<""_t>, optional<"legacy"_t>>{ "nrequired", "keys", "label", "address_type" },
        ////method<"backupwallet", string_t>{ "destination" },
        ////method<"bumpfee", string_t, optional<empty::object>>{ "txid", "options" },
        ////method<"createwallet", string_t, optional<false>, optional<false>, optional<false>, optional<""_t>, optional<false>, optional<false>, optional<false>, optional<"set"_t>>{ "wallet_name", "disable_private_keys", "blank", "passphrase", "avoid_reuse", "descriptors", "load_on_startup", "external_signer", "change_type" },
        ////method<"dumpprivkey", string_t>{ "address" },
        ////method<"dumpwallet", string_t>{ "filename" },
        ////method<"encryptwallet", string_t>{ "passphrase" },
        ////method<"getaddressinfo", string_t>{ "address" },
        ////method<"getbalances">{},
        ////method<"getnewaddress", optional<""_t>, optional<"legacy"_t>>{ "label", "address_type" },
        ////method<"getreceivedbyaddress", string_t, optional<0>>{ "address", "minconf" },
        ////method<"getreceivedbylabel", string_t, optional<0>>{ "label", "minconf" },
        ////method<"gettransaction", string_t, optional<false>, optional<false>>{ "txid", "include_watchonly", "verbose" },
        ////method<"getunconfirmedbalance">{},
        ////method<"getwalletinfo">{},
        ////method<"importaddress", string_t, optional<""_t>, optional<false>, optional<true>>{ "address", "label", "rescan", "p2sh" },
        ////method<"importmulti", array_t, optional<empty::object>>{ "requests", "options" },
        ////method<"importprivkey", string_t, optional<""_t>, optional<false>>{ "privkey", "label", "rescan" },
        ////method<"importprunedfunds", string_t, string_t>{ "rawtransaction", "txoutproof" },
        ////method<"importpubkey", string_t, optional<""_t>, optional<false>>{ "pubkey", "label", "rescan" },
        ////method<"importwallet", string_t>{ "filename" },
        ////method<"keypoolrefill", optional<100>>{ "newsize" },
        ////method<"listaddressgroupings", optional<1>, optional<false>>{ "minconf", "include_watchonly" },
        ////method<"listlabels", optional<"receive"_t>>{ "purpose" },
        ////method<"listlockunspent">{},
        ////method<"listreceivedbyaddress", optional<1>, optional<false>, optional<false>, optional<""_t>>{ "minconf", "include_empty", "include_watchonly", "address_filter" },
        ////method<"listreceivedbylabel", optional<1>, optional<false>, optional<false>>{ "minconf", "include_empty", "include_watchonly" },
        ////method<"listtransactions", optional<""_t>, optional<10>, optional<0>, optional<false>>{ "label", "count", "skip", "include_watchonly" },
        ////method<"list_unspent", optional<1>, optional<empty::array>, optional<true>, optional<false>>{ "minconf", "addresses", "include_unsafe", "query_options" },
        ////method<"loadwallet", string_t, optional<false>>{ "filename", "load_on_startup" },
        ////method<"lockunspent", boolean_t, optional<empty::array>>{ "unlock", "transactions" },
        ////method<"removeprunedfunds", string_t>{ "txid" },
        ////method<"rescanblockchain", optional<0>>{ "start_height" },
        ////method<"send", object_t, optional<empty::object>>{ "outputs", "options" },
        ////method<"sendmany", string_t, object_t, optional<1>, optional<""_t>, optional<""_t>, optional<false>, optional<false>, optional<25>, optional<"unset"_t>, optional<false>, optional<0>>{ "dummy", "outputs", "minconf", "comment", "comment_to", "subtractfeefrom", "replaceable", "conf_target", "estimate_mode", "avoid_reuse", "fee_rate" },
        ////method<"sendtoaddress", string_t, number_t, optional<""_t>, optional<""_t>, optional<false>, optional<25>, optional<"unset"_t>, optional<false>, optional<0>, optional<false>>{ "address", "amount", "comment", "comment_to", "subtractfeefromamount", "conf_target", "estimate_mode", "avoid_reuse", "fee_rate", "verbose" },
        ////method<"setlabel", string_t, string_t>{ "address", "label" },
        ////method<"settxfee", number_t>{ "amount" },
        ////method<"signmessage", string_t, string_t>{ "address", "message" },
        ////method<"signmessagewithprivkey", string_t, string_t>{ "privkey", "message" },
        ////method<"syncwithvalidationinterfacequeue">{},
        ////method<"unloadwallet", optional<""_t>, optional<false>>{ "wallet_name", "load_on_startup" },
        ////method<"walletcreatefundedpsbt", optional<empty::array>, optional<empty::object>, optional<0>, optional<empty::object>>{ "inputs", "outputs", "locktime", "options" },
        ////method<"walletlock">{},
        ////method<"walletpassphrase", string_t, number_t>{ "passphrase", "timeout" },
        ////method<"walletprocesspsbt", string_t, optional<false>, optional<false>, optional<false>>{ "psbt", "sign", "bip32derivs", "complete" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using get_best_block_hash = at<0>;
    using get_block = at<1>;
    using get_block_chain_info = at<2>;
    using get_block_count = at<3>;
    using get_block_filter = at<4>;
    using get_block_hash = at<5>;
    using get_block_header = at<6>;
    using get_block_stats = at<7>;
    using get_chain_tx_stats = at<8>;
    using get_chain_work = at<9>;
    using get_tx_out = at<10>;
    using get_tx_out_set_info = at<11>;
    using prune_block_chain = at<12>;
    using save_mem_pool = at<13>;
    using scan_tx_out_set = at<14>;
    using verify_chain = at<15>;
    using verify_tx_out_set = at<16>;

    ////using get_memory_info = at<17>;
    ////using get_rpc_info = at<18>;
    ////using help = at<19>;
    ////using logging = at<20>;
    ////using stop = at<21>;
    ////using uptime = at<22>;
    ////using get_block_template = at<23>;
    ////using get_mining_info = at<24>;
    ////using get_network_hash_ps = at<25>;
    ////using prioritise_transaction = at<26>;
    ////using submit_block = at<27>;
    ////using add_node = at<28>;
    ////using clear_banned = at<29>;
    ////using disconnect_node = at<30>;
    ////using get_added_node_info = at<31>;
    ////using get_connection_count = at<32>;
    using get_network_info = at<17>;
    ////using get_peer_info = at<34>;
    ////using list_banned = at<35>;
    ////using ping = at<36>;
    ////using set_ban = at<37>;
    ////using set_network_active = at<38>;
    ////using combine_raw_transaction = at<39>;
    ////using create_raw_transaction = at<40>;
    ////using decode_raw_transaction = at<41>;
    ////using fund_raw_transaction = at<42>;
    ////using get_raw_transaction = at<43>;
    ////using send_raw_transaction = at<44>;
    ////using sign_raw_transaction_with_key = at<45>;
    ////using test_mem_pool_accept = at<46>;
    ////using test_raw_transaction = at<47>;
    ////using create_multisig = at<48>;
    ////using decode_psbt = at<49>;
    ////using decode_script = at<50>;
    ////using estimate_raw_fee = at<51>;
    ////using get_descriptor_info = at<52>;
    ////using validate_address = at<53>;
    ////using abandon_transaction = at<54>;
    ////using add_multisig_address = at<55>;
    ////using backup_wallet = at<56>;
    ////using bump_fee = at<57>;
    ////using create_wallet = at<58>;
    ////using dump_priv_key = at<59>;
    ////using dump_wallet = at<60>;
    ////using encrypt_wallet = at<61>;
    ////using get_address_info = at<62>;
    ////using get_balances = at<63>;
    ////using get_new_address = at<64>;
    ////using get_received_by_address = at<65>;
    ////using get_received_by_label = at<66>;
    ////using get_transaction = at<67>;
    ////using get_unconfirmed_balance = at<68>;
    ////using get_wallet_info = at<69>;
    ////using import_address = at<70>;
    ////using import_multi = at<71>;
    ////using import_priv_key = at<72>;
    ////using import_pruned_funds = at<73>;
    ////using import_pub_key = at<74>;
    ////using import_wallet = at<75>;
    ////using key_pool_refill = at<76>;
    ////using list_address_groupings = at<77>;
    ////using list_labels = at<78>;
    ////using list_lock_unspent = at<79>;
    ////using list_received_by_address = at<80>;
    ////using list_received_by_label = at<81>;
    ////using list_transactions = at<82>;
    ////using list_unspent = at<83>;
    ////using load_wallet = at<84>;
    ////using lock_unspent = at<85>;
    ////using remove_pruned_funds = at<86>;
    ////using rescan_block_chain = at<87>;
    ////using send = at<88>;
    ////using send_many = at<89>;
    ////using send_to_address = at<90>;
    ////using set_label = at<91>;
    ////using set_tx_fee = at<92>;
    ////using sign_message = at<93>;
    ////using sign_message_with_priv_key = at<94>;
    ////using sync_with_validation_interface_queue = at<95>;
    ////using unload_wallet = at<96>;
    ////using wallet_create_funded_psbt = at<97>;
    ////using wallet_lock = at<98>;
    ////using wallet_passphrase = at<99>;
    ////using wallet_process_psbt = at<100>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
