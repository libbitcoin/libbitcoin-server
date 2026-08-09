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
