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
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
