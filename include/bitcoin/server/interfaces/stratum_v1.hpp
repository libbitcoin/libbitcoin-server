/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_INTERFACES_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_INTERFACES_STRATUM_V1_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct stratum_v1_methods
{
    static constexpr std::tuple methods
    {
        /// Client requests.
        method<"mining.subscribe", optional<""_t>, optional<0.0>>{ "user_agent", "extranonce1_size" },
        method<"mining.authorize", string_t, string_t>{ "username", "password" },
        method<"mining.submit", string_t, string_t, string_t, number_t, string_t>{ "worker_name", "job_id", "extranonce2", "ntime", "nonce" },
        method<"mining.extranonce.subscribe">{},
        method<"mining.extranonce.unsubscribe", number_t>{ "id" },

        /// Server notifications.
        method<"mining.configure", object_t>{ "extensions" },
        method<"mining.set_difficulty", optional<1.0>>{ "difficulty" },
        method<"mining.notify", string_t, string_t, string_t, string_t, array_t, number_t, number_t, number_t, boolean_t, boolean_t, boolean_t>{ "job_id", "prevhash", "coinb1", "coinb2", "merkle_branch", "version", "nbits", "ntime", "clean_jobs", "hash1", "hash2" },
        method<"client.reconnect", string_t, number_t, number_t>{ "url", "port", "id" },
        method<"client.hello", object_t>{ "protocol" },
        method<"client.rejected", string_t, string_t>{ "job_id", "reject_reason" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using mining_subscribe = at<0>;
    using mining_authorize = at<1>;
    using mining_submit = at<2>;
    using mining_extranonce_subscribe = at<3>;
    using mining_extranonce_unsubscribe = at<4>;
    using mining_configure = at<5>;
    using mining_set_difficulty = at<6>;
    using mining_notify = at<7>;
    using client_reconnect = at<8>;
    using client_hello = at<9>;
    using client_rejected = at<10>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
