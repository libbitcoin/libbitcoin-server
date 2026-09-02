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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_TEST_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_TEST_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_test_methods
{
    static constexpr std::tuple methods
    {
        method<"addconnection", string_t, string_t, boolean_t>{ unimplemented, "address", "connection_type", "v2transport" },
        method<"addpeeraddress", string_t, number_t, nullopt<false>>{ unimplemented, "address", "port", "tried" },
        method<"echo", nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>>{ unimplemented, "arg0", "arg1", "arg2", "arg3", "arg4", "arg5", "arg6", "arg7", "arg8", "arg9" },
        method<"echoipc", string_t>{ unimplemented, "arg" },
        method<"echojson", nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>, nullable<value_t>>{ unimplemented, "arg0", "arg1", "arg2", "arg3", "arg4", "arg5", "arg6", "arg7", "arg8", "arg9" },
        method<"estimaterawfee", number_t, nullopt<0.95>>{ unimplemented, "conf_target", "threshold" },
        method<"generate">{ unimplemented },
        method<"generateblock", string_t, array_t, nullopt<true>>{ unimplemented, "output", "transactions", "submit" },
        method<"generatetoaddress", number_t, string_t, nullopt<1000000.0>>{ unimplemented, "nblocks", "address", "maxtries" },
        method<"generatetodescriptor", number_t, string_t, nullopt<1000000.0>>{ unimplemented, "num_blocks", "descriptor", "maxtries" },
        method<"getmempoolfeeratediagram">{ unimplemented },
        method<"getorphantxs", nullopt<0.0>>{ unimplemented, "verbosity" },
        method<"getrawaddrman">{ unimplemented },
        method<"invalidateblock", string_t>{ unimplemented, "blockhash" },
        method<"mockscheduler", number_t>{ unimplemented, "delta_time" },
        method<"reconsiderblock", string_t>{ unimplemented, "blockhash" },
        method<"sendmsgtopeer", number_t, string_t, string_t>{ unimplemented, "peer_id", "msg_type", "msg" },
        method<"setmocktime", number_t>{ unimplemented, "timestamp" },
        method<"syncwithvalidationinterfacequeue">{ unimplemented }
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
    using add_connection = at<0>;
    using add_peer_address = at<1>;
    using echo = at<2>;
    using echo_ipc = at<3>;
    using echo_json = at<4>;
    using estimate_raw_fee = at<5>;
    using generate = at<6>;
    using generate_block = at<7>;
    using generate_to_address = at<8>;
    using generate_to_descriptor = at<9>;
    using get_mempool_fee_rate_diagram = at<10>;
    using get_orphan_txs = at<11>;
    using get_raw_addrman = at<12>;
    using invalidate_block = at<13>;
    using mock_scheduler = at<14>;
    using reconsider_block = at<15>;
    using send_msg_to_peer = at<16>;
    using set_mock_time = at<17>;
    using sync_with_validation_interface_queue = at<18>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
