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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_UTILITY_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_UTILITY_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct bitcoind_utility_methods
{
    static constexpr std::tuple methods
    {
        method<"decodescript", string_t>{ "hexstring" },
        method<"validateaddress", string_t>{ "address" },
        method<"createmultisig", number_t, array_t, nullopt<"legacy"_t>>{ "nrequired", "keys", "address_type" },
        method<"deriveaddresses", string_t, nullable<value_t>>{ "descriptor", "range" },
        method<"getdescriptorinfo", string_t>{ "descriptor" },
        method<"verifymessage", string_t, string_t, string_t>{ "address", "signature", "message" },
        method<"getindexinfo", nullopt<""_t>>{ "index_name" },
        method<"estimatesmartfee", number_t, nullopt<"economical"_t>, nullable<object_t>>{ "conf_target", "estimate_mode", "options" },
        method<"signmessagewithprivkey", string_t, string_t>{ unimplemented, "privkey", "message" }
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
    using decode_script = at<0>;
    using validate_address = at<1>;
    using create_multisig = at<2>;
    using derive_addresses = at<3>;
    using get_descriptor_info = at<4>;
    using verify_message = at<5>;
    using get_index_info = at<6>;
    using estimate_smart_fee = at<7>;
    using sign_message_with_priv_key = at<8>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
