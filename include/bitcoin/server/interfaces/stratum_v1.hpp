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
#ifndef LIBBITCOIN_SERVER_INTERFACES_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_INTERFACES_STRATUM_V1_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct stratum_v1_methods
{
    /// Methods requested by the client (notifications are sent by name).
    static constexpr std::tuple methods
    {
        method<"mining.subscribe", nullopt<""_t>, nullopt<""_t>, nullopt<""_t>, nullopt<empty::value>>{ "user_agent", "session_id", "host", "port" },
        method<"mining.authorize", string_t, nullopt<""_t>>{ "username", "password" },
        method<"mining.submit", string_t, string_t, string_t, string_t, string_t, optional<""_t>>{ "worker_name", "job_id", "extranonce2", "ntime", "nonce", "version_bits" },
        method<"mining.configure", array_t, object_t>{ "extensions", "parameters" },
        method<"mining.suggest_difficulty", number_t>{ "difficulty" },
        method<"mining.suggest_target", string_t>{ "target" },
        method<"mining.get_transactions", string_t>{ "job_id" },
        method<"mining.multi_version", number_t>{ "version" },
        method<"mining.extranonce.subscribe">{}
    };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using mining_subscribe = at<0>;
    using mining_authorize = at<1>;
    using mining_submit = at<2>;
    using mining_configure = at<3>;
    using mining_suggest_difficulty = at<4>;
    using mining_suggest_target = at<5>;
    using mining_get_transactions = at<6>;
    using mining_multi_version = at<7>;
    using mining_extranonce_subscribe = at<8>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
