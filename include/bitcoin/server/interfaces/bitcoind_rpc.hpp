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
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
