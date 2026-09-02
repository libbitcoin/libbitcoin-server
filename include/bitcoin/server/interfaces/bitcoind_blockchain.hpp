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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_BLOCKCHAIN_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_BLOCKCHAIN_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_blockchain_methods
{
    static constexpr std::tuple methods
    {
        method<"getbestblockhash">{},
        method<"getblock", string_t, nullopt<1.0>>{ "blockhash", "verbosity" },
        method<"getblockchaininfo">{},
        method<"getblockcount">{},
        method<"getblockfilter", string_t, nullopt<"basic"_t>>{ "blockhash", "filtertype" },
        method<"getblockhash", number_t>{ "height" },
        method<"getblockheader", string_t, nullopt<true>>{ "blockhash", "verbose" },
        method<"getblockstats", value_t, nullopt<empty::array>>{ "hash_or_height", "stats" },
        method<"getchaintxstats", nullopt<-1.0>, nullopt<""_t>>{ "nblocks", "blockhash" },
        method<"gettxout", string_t, number_t, nullopt<true>>{ "txid", "n", "include_mempool" },
        method<"gettxoutsetinfo", nullopt<"hash_serialized_3"_t>, nullopt<empty::value>, nullopt<true>>{ "hash_type", "hash_or_height", "use_index" },
        method<"pruneblockchain", number_t>{ unimplemented, "height" },
        method<"savemempool">{ unimplemented },
        method<"scantxoutset", string_t, nullopt<empty::array>>{ "action", "scanobjects" },
        method<"verifychain", nullopt<3.0>, nullopt<6.0>>{ "checklevel", "nblocks" },
        method<"dumptxoutset", string_t, nullopt<""_t>, nullable<value_t>>{ unimplemented, "path", "type", "options" },
        method<"loadtxoutset", string_t>{ unimplemented, "path" },
        method<"gettxoutproof", array_t, nullopt<""_t>>{ "txids", "blockhash" },
        method<"verifytxoutproof", string_t>{ "proof" },
        method<"getblockfrompeer", string_t, number_t>{ unimplemented, "blockhash", "peer_id" },
        method<"getchainstates">{},
        method<"getchaintips">{},
        method<"getdeploymentinfo", nullopt<""_t>>{ "blockhash" },
        method<"getdescriptoractivity", nullopt<empty::array>, nullopt<empty::array>, nullopt<true>>{ "blockhashes", "scanobjects", "include_mempool" },
        method<"getdifficulty">{},
        method<"preciousblock", string_t>{ "blockhash" },
        method<"scanblocks", string_t, nullopt<empty::array>, nullopt<0.0>, nullopt<-1.0>, nullopt<"basic"_t>, nullopt<empty::object>>{ "action", "scanobjects", "start_height", "stop_height", "filtertype", "options" },
        method<"waitforblock", string_t, nullopt<0.0>>{ "blockhash", "timeout" },
        method<"waitforblockheight", number_t, nullopt<0.0>>{ "height", "timeout" },
        method<"waitfornewblock", nullopt<0.0>, nullopt<""_t>>{ "timeout", "current_tip" },
        method<"getmempoolancestors", string_t, nullopt<false>>{ unimplemented, "txid", "verbose" },
        method<"getmempoolcluster", string_t>{ unimplemented, "txid" },
        method<"getmempooldescendants", string_t, nullopt<false>>{ unimplemented, "txid", "verbose" },
        method<"getmempoolentry", string_t>{ unimplemented, "txid" },
        method<"getmempoolinfo">{ unimplemented },
        method<"getrawmempool", nullopt<false>, nullopt<false>>{ unimplemented, "verbose", "mempool_sequence" },
        method<"gettxspendingprevout", array_t, nullable<object_t>>{ unimplemented, "outputs", "options" },
        method<"importmempool", string_t, nullable<object_t>>{ unimplemented, "filepath", "options" }
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
    using dump_tx_out_set = at<15>;
    using load_tx_out_set = at<16>;
    using get_tx_out_proof = at<17>;
    using verify_tx_out_proof = at<18>;
    using get_block_from_peer = at<19>;
    using get_chain_states = at<20>;
    using get_chain_tips = at<21>;
    using get_deployment_info = at<22>;
    using get_descriptor_activity = at<23>;
    using get_difficulty = at<24>;
    using precious_block = at<25>;
    using scan_blocks = at<26>;
    using wait_for_block = at<27>;
    using wait_for_block_height = at<28>;
    using wait_for_new_block = at<29>;
    using get_mempool_ancestors = at<30>;
    using get_mempool_cluster = at<31>;
    using get_mempool_descendants = at<32>;
    using get_mempool_entry = at<33>;
    using get_mempool_info = at<34>;
    using get_raw_mempool = at<35>;
    using get_tx_spending_prevout = at<36>;
    using import_mempool = at<37>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
