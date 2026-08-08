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
        method<"getblockstats", value_t, optional<empty::array>>{ unimplemented, "hash_or_height", "stats" },
        method<"getchaintxstats", optional<-1.0>, optional<""_t>>{ unimplemented, "nblocks", "blockhash" },
        method<"gettxout", string_t, number_t, optional<true>>{ "txid", "n", "include_mempool" },
        method<"gettxoutsetinfo">{ unimplemented },
        method<"pruneblockchain", number_t>{ unimplemented, "height" },
        method<"savemempool">{ unimplemented },
        method<"scantxoutset", string_t, optional<empty::array>>{ unimplemented, "action", "scanobjects" },
        method<"verifychain", optional<4.0>, optional<288.0>>{ "checklevel", "nblocks" },

        method<"help", optional<""_t>>{ "command" },

        method<"getnetworkhashps", optional<120_u32>, optional<-1_i32>>{ "nblocks", "height" },

        method<"getnetworkinfo">{},

        /// Rawtransactions methods (implemented).
        method<"createrawtransaction", array_t, object_t, optional<0_u32>, optional<false>>{ "inputs", "outputs", "locktime", "replaceable" },
        method<"decoderawtransaction", string_t>{ "hexstring" },
        method<"getrawtransaction", string_t, optional<0.0>, optional<""_t>>{ "txid", "verbosity", "blockhash" },
        method<"sendrawtransaction", string_t, optional<0.0>>{ "hexstring", "maxfeerate" },
        method<"testmempoolaccept", array_t, optional<0_u32>>{ "rawtxs", "maxfeerate" },

        /// Util methods (implemented).
        method<"decodescript", string_t>{ "hex" },
        method<"validateaddress", string_t>{ "address" },

        // Unimplemented (dispatchable, refused).
        method<"dumptxoutset">{ unimplemented },
        method<"loadtxoutset">{ unimplemented },
        method<"clearbanned">{ unimplemented },
        method<"listbanned">{ unimplemented },
        method<"setban">{ unimplemented },
        method<"stop">{ unimplemented },
        method<"gettxoutproof">{ unimplemented },
        method<"verifytxoutproof">{ unimplemented },
        method<"getblockfrompeer">{ unimplemented },
        method<"getchainstates">{ unimplemented },
        method<"getchaintips">{ unimplemented },
        method<"getdeploymentinfo">{ unimplemented },
        method<"getdescriptoractivity">{ unimplemented },
        method<"getdifficulty">{ unimplemented },
        method<"preciousblock">{ unimplemented },
        method<"scanblocks">{ unimplemented },
        method<"waitforblock">{ unimplemented },
        method<"waitforblockheight">{ unimplemented },
        method<"waitfornewblock">{ unimplemented },
        method<"analyzepsbt">{ unimplemented },
        method<"combinepsbt">{ unimplemented },
        method<"converttopsbt">{ unimplemented },
        method<"createpsbt">{ unimplemented },
        method<"decodepsbt">{ unimplemented },
        method<"finalizepsbt">{ unimplemented },
        method<"joinpsbts">{ unimplemented },
        method<"descriptorprocesspsbt">{ unimplemented },
        method<"utxoupdatepsbt">{ unimplemented },
        method<"getmininginfo">{ unimplemented },
        method<"submitblock">{ unimplemented },
        method<"submitheader">{ unimplemented },
        method<"addnode">{ unimplemented },
        method<"disconnectnode">{ unimplemented },
        method<"exportasmap">{ unimplemented },
        method<"getaddednodeinfo">{ unimplemented },
        method<"getaddrmaninfo">{ unimplemented },
        method<"getconnectioncount">{ unimplemented },
        method<"getnettotals">{ unimplemented },
        method<"getnodeaddresses">{ unimplemented },
        method<"getpeerinfo">{ unimplemented },
        method<"ping">{ unimplemented },
        method<"setnetworkactive">{ unimplemented },
        method<"createmultisig">{ unimplemented },
        method<"deriveaddresses">{ unimplemented },
        method<"getdescriptorinfo">{ unimplemented },
        method<"verifymessage">{ unimplemented },
        method<"getindexinfo">{ unimplemented },
        method<"getmemoryinfo">{ unimplemented },
        method<"getopenrpcinfo">{ unimplemented },
        method<"getrpcinfo">{ unimplemented },
        method<"logging">{ unimplemented },
        method<"uptime">{ unimplemented },
        method<"getzmqnotifications">{ unimplemented },
        method<"enumeratesigners">{ unimplemented },
        method<"addconnection">{ unimplemented },
        method<"addpeeraddress">{ unimplemented },
        method<"echo">{ unimplemented },
        method<"echoipc">{ unimplemented },
        method<"echojson">{ unimplemented },
        method<"estimaterawfee">{ unimplemented },
        method<"generate">{ unimplemented },
        method<"generateblock">{ unimplemented },
        method<"generatetoaddress">{ unimplemented },
        method<"generatetodescriptor">{ unimplemented },
        method<"getmempoolfeeratediagram">{ unimplemented },
        method<"getorphantxs">{ unimplemented },
        method<"getrawaddrman">{ unimplemented },
        method<"invalidateblock">{ unimplemented },
        method<"mockscheduler">{ unimplemented },
        method<"reconsiderblock">{ unimplemented },
        method<"sendmsgtopeer">{ unimplemented },
        method<"setmocktime">{ unimplemented },
        method<"syncwithvalidationinterfacequeue">{ unimplemented },
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
        method<"walletprocesspsbt">{ unimplemented },
        method<"getmempoolancestors">{ unimplemented },
        method<"getmempoolcluster">{ unimplemented },
        method<"getmempooldescendants">{ unimplemented },
        method<"getmempoolentry">{ unimplemented },
        method<"getmempoolinfo">{ unimplemented },
        method<"getrawmempool">{ unimplemented },
        method<"gettxspendingprevout">{ unimplemented },
        method<"importmempool">{ unimplemented },
        method<"abortprivatebroadcast">{ unimplemented },
        method<"getprivatebroadcastinfo">{ unimplemented },
        method<"submitpackage">{ unimplemented },
        method<"getblocktemplate">{ unimplemented },
        method<"getprioritisedtransactions">{ unimplemented },
        method<"prioritisetransaction">{ unimplemented },
        method<"estimatesmartfee">{ unimplemented },
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
    using get_best_block_hash = at<0>;
    using get_block = at<1>;
    using get_block_chain_info = at<2>;
    using get_block_count = at<3>;
    using get_block_filter = at<4>;
    using get_block_hash = at<5>;
    using get_block_header = at<6>;
    using get_block_stats = at<7>;
    using get_chain_tx_stats = at<8>;
    using get_tx_out = at<9>;
    using get_tx_out_set_info = at<10>;
    using prune_block_chain = at<11>;
    using save_mempool = at<12>;
    using scan_tx_out_set = at<13>;
    using verify_chain = at<14>;

    using help = at<15>;
    using get_network_hash_ps = at<16>;
    using get_network_info = at<17>;
    using create_raw_transaction = at<18>;
    using decode_raw_transaction = at<19>;
    using get_raw_transaction = at<20>;
    using send_raw_transaction = at<21>;
    using test_mempool_accept = at<22>;
    using decode_script = at<23>;
    using validate_address = at<24>;
    using dump_tx_out_set = at<25>;
    using load_tx_out_set = at<26>;
    using clear_banned = at<27>;
    using list_banned = at<28>;
    using set_ban = at<29>;
    using stop = at<30>;
    using get_tx_out_proof = at<31>;
    using verify_tx_out_proof = at<32>;
    using get_block_from_peer = at<33>;
    using get_chain_states = at<34>;
    using get_chain_tips = at<35>;
    using get_deployment_info = at<36>;
    using get_descriptor_activity = at<37>;
    using get_difficulty = at<38>;
    using precious_block = at<39>;
    using scan_blocks = at<40>;
    using wait_for_block = at<41>;
    using wait_for_block_height = at<42>;
    using wait_for_new_block = at<43>;
    using analyze_psbt = at<44>;
    using combine_psbt = at<45>;
    using convert_to_psbt = at<46>;
    using create_psbt = at<47>;
    using decode_psbt = at<48>;
    using finalize_psbt = at<49>;
    using join_psbts = at<50>;
    using descriptor_process_psbt = at<51>;
    using utxo_update_psbt = at<52>;
    using get_mining_info = at<53>;
    using submit_block = at<54>;
    using submit_header = at<55>;
    using add_node = at<56>;
    using disconnect_node = at<57>;
    using export_asmap = at<58>;
    using get_added_node_info = at<59>;
    using get_addrman_info = at<60>;
    using get_connection_count = at<61>;
    using get_net_totals = at<62>;
    using get_node_addresses = at<63>;
    using get_peer_info = at<64>;
    using ping = at<65>;
    using set_network_active = at<66>;
    using create_multisig = at<67>;
    using derive_addresses = at<68>;
    using get_descriptor_info = at<69>;
    using verify_message = at<70>;
    using get_index_info = at<71>;
    using get_memory_info = at<72>;
    using get_openrpc_info = at<73>;
    using get_rpc_info = at<74>;
    using logging = at<75>;
    using uptime = at<76>;
    using get_zmq_notifications = at<77>;
    using enumerate_signers = at<78>;
    using add_connection = at<79>;
    using add_peer_address = at<80>;
    using echo = at<81>;
    using echo_ipc = at<82>;
    using echo_json = at<83>;
    using estimate_raw_fee = at<84>;
    using generate = at<85>;
    using generate_block = at<86>;
    using generate_to_address = at<87>;
    using generate_to_descriptor = at<88>;
    using get_mempool_fee_rate_diagram = at<89>;
    using get_orphan_txs = at<90>;
    using get_raw_addrman = at<91>;
    using invalidate_block = at<92>;
    using mock_scheduler = at<93>;
    using reconsider_block = at<94>;
    using send_msg_to_peer = at<95>;
    using set_mock_time = at<96>;
    using sync_with_validation_interface_queue = at<97>;
    using abandon_transaction = at<98>;
    using abort_rescan = at<99>;
    using add_hd_key = at<100>;
    using backup_wallet = at<101>;
    using bump_fee = at<102>;
    using create_wallet = at<103>;
    using create_wallet_descriptor = at<104>;
    using encrypt_wallet = at<105>;
    using export_watchonly_wallet = at<106>;
    using get_addresses_by_label = at<107>;
    using get_address_info = at<108>;
    using get_balance = at<109>;
    using get_balances = at<110>;
    using get_hd_keys = at<111>;
    using get_new_address = at<112>;
    using get_raw_change_address = at<113>;
    using get_received_by_address = at<114>;
    using get_received_by_label = at<115>;
    using get_transaction = at<116>;
    using get_wallet_info = at<117>;
    using import_descriptors = at<118>;
    using import_pruned_funds = at<119>;
    using keypool_refill = at<120>;
    using list_address_groupings = at<121>;
    using list_descriptors = at<122>;
    using list_labels = at<123>;
    using list_lock_unspent = at<124>;
    using list_received_by_address = at<125>;
    using list_received_by_label = at<126>;
    using list_since_block = at<127>;
    using list_transactions = at<128>;
    using list_unspent = at<129>;
    using list_wallet_dir = at<130>;
    using list_wallets = at<131>;
    using load_wallet = at<132>;
    using lock_unspent = at<133>;
    using migrate_wallet = at<134>;
    using psbt_bump_fee = at<135>;
    using remove_pruned_funds = at<136>;
    using rescan_block_chain = at<137>;
    using restore_wallet = at<138>;
    using send = at<139>;
    using send_all = at<140>;
    using send_many = at<141>;
    using send_to_address = at<142>;
    using set_label = at<143>;
    using set_wallet_flag = at<144>;
    using sign_message = at<145>;
    using sign_raw_transaction_with_wallet = at<146>;
    using simulate_raw_transaction = at<147>;
    using unload_wallet = at<148>;
    using wallet_create_funded_psbt = at<149>;
    using wallet_display_address = at<150>;
    using wallet_lock = at<151>;
    using wallet_passphrase = at<152>;
    using wallet_passphrase_change = at<153>;
    using wallet_process_psbt = at<154>;
    using get_mempool_ancestors = at<155>;
    using get_mempool_cluster = at<156>;
    using get_mempool_descendants = at<157>;
    using get_mempool_entry = at<158>;
    using get_mempool_info = at<159>;
    using get_raw_mempool = at<160>;
    using get_tx_spending_prevout = at<161>;
    using import_mempool = at<162>;
    using abort_private_broadcast = at<163>;
    using get_private_broadcast_info = at<164>;
    using submit_package = at<165>;
    using get_block_template = at<166>;
    using get_prioritised_transactions = at<167>;
    using prioritise_transaction = at<168>;
    using estimate_smart_fee = at<169>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
