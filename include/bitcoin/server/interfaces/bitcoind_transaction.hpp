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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_TRANSACTION_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_TRANSACTION_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_transaction_methods
{
    static constexpr std::tuple methods
    {
        method<"createrawtransaction", array_t, value_t, optional<0.0>, optional<true>, optional<2.0>>{ "inputs", "outputs", "locktime", "replaceable", "version" },
        method<"decoderawtransaction", string_t, nullable<boolean_t>>{ "hexstring", "iswitness" },
        method<"getrawtransaction", string_t, optional<0.0>, optional<""_t>>{ "txid", "verbosity", "blockhash" },
        // bitcoind also accepts quoted amounts (slop; needs dispatch special case).
        method<"sendrawtransaction", string_t, optional<0.1>, optional<0.0>>{ "hexstring", "maxfeerate", "maxburnamount" },
        method<"testmempoolaccept", array_t, optional<0.1>>{ "rawtxs", "maxfeerate" },
        method<"analyzepsbt", string_t>{ "psbt" },
        method<"combinepsbt", array_t>{ "txs" },
        method<"converttopsbt", string_t, optional<false>, nullable<boolean_t>, optional<2.0>>{ "hexstring", "permitsigdata", "iswitness", "psbt_version" },
        method<"createpsbt", array_t, value_t, optional<0.0>, optional<true>, optional<2.0>, optional<2.0>>{ "inputs", "outputs", "locktime", "replaceable", "version", "psbt_version" },
        method<"decodepsbt", string_t>{ "psbt" },
        method<"finalizepsbt", string_t, optional<true>>{ "psbt", "extract" },
        method<"joinpsbts", array_t>{ "txs" },
        method<"descriptorprocesspsbt">{ unimplemented },
        method<"utxoupdatepsbt", string_t, optional<empty::array>>{ "psbt", "descriptors" },
        method<"abortprivatebroadcast">{ unimplemented },
        method<"getprivatebroadcastinfo">{ unimplemented },
        method<"submitpackage">{ unimplemented },
        method<"combinerawtransaction", array_t>{ "txs" },
        method<"signrawtransactionwithkey">{ unimplemented }
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
    using create_raw_transaction = at<0>;
    using decode_raw_transaction = at<1>;
    using get_raw_transaction = at<2>;
    using send_raw_transaction = at<3>;
    using test_mempool_accept = at<4>;
    using analyze_psbt = at<5>;
    using combine_psbt = at<6>;
    using convert_to_psbt = at<7>;
    using create_psbt = at<8>;
    using decode_psbt = at<9>;
    using finalize_psbt = at<10>;
    using join_psbts = at<11>;
    using descriptor_process_psbt = at<12>;
    using utxo_update_psbt = at<13>;
    using abort_private_broadcast = at<14>;
    using get_private_broadcast_info = at<15>;
    using submit_package = at<16>;
    using combine_raw_transaction = at<17>;
    using sign_raw_transaction_with_key = at<18>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
