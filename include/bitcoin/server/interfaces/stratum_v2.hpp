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
#ifndef LIBBITCOIN_SERVER_INTERFACES_STRATUM_V2_HPP
#define LIBBITCOIN_SERVER_INTERFACES_STRATUM_V2_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct stratum_v2_methods
{
    static constexpr std::tuple methods
    {
        /// Common setup messages.
        method<"setup_connection", number_t, number_t, number_t, number_t, string_t, string_t, string_t>{ "min_version", "max_version", "flags", "endpoint_port", "endpoint_host", "vendor", "user_agent" },
        method<"setup_connection_success", number_t, number_t>{ "used_version", "flags" },
        method<"setup_connection_error", string_t>{ "code" },

        /// Standard mining channel (header-only, primary for devices/firmware).
        method<"open_standard_mining_channel", string_t, number_t, string_t, number_t>{ "user_identity", "nominal_hash_rate", "max_target", "min_extranonce_size" },
        method<"open_standard_mining_channel_success", number_t, string_t, string_t, number_t>{ "channel_id", "target", "extranonce_prefix", "extranonce_size" },
        method<"open_standard_mining_channel_error", string_t>{ "code" },

        /// Extended mining channel.
        method<"open_extended_mining_channel", string_t, number_t, string_t, number_t>{ "user_identity", "nominal_hash_rate", "max_target", "min_extranonce_size" },
        method<"open_extended_mining_channel_success", number_t, string_t, string_t, number_t, number_t, number_t>{ "channel_id", "target", "extranonce_prefix", "extranonce_size", "max_coinbase_size", "coinbase_output_max_additional_size" },
        method<"open_extended_mining_channel_error", string_t>{ "code" },

        /// Channel management.
        method<"update_channel", number_t, number_t, optional<""_t>>{ "channel_id", "nominal_hash_rate", "max_target" },
        method<"update_channel_error", number_t, string_t>{ "channel_id", "code" },
        method<"close_channel", number_t, optional<""_t>>{ "channel_id", "reason" },
        method<"set_target", number_t, string_t>{ "channel_id", "maximum_target" },

        /// Header-only job declaration.
        method<"new_mining_job", number_t, boolean_t, number_t, string_t, string_t, number_t>{ "job_id", "future_job", "version", "prev_hash", "merkle_root", "ntime_offset" },
        method<"set_new_prev_hash", number_t, number_t, string_t, number_t, number_t>{ "channel_id", "job_id", "prev_hash", "min_ntime", "nbits" },

        /// Extended / custom jobs.
        method<"new_extended_mining_job", number_t, boolean_t, number_t, string_t, string_t, array_t>{ "job_id", "future_job", "version", "coinbase_prefix", "coinbase_suffix", "merkle_path" },
        method<"set_custom_mining_job", number_t, boolean_t, number_t, string_t, number_t, number_t, array_t, array_t>{ "channel_id", "future_job", "job_id", "prev_hash", "version", "ntime", "merkle_branch", "transactions" },
        method<"set_custom_mining_job_success", number_t>{ "job_id" },
        method<"set_custom_mining_job_error", number_t, string_t>{ "job_id", "code" },

        /// Share submission.
        method<"submit_shares", number_t, number_t, number_t, number_t, number_t, number_t>{ "channel_id", "sequence_number", "job_id", "nonce", "ntime", "version" },
        method<"submit_shares_success", number_t, optional<""_t>>{ "sequence_number", "new_target" },
        method<"submit_shares_error", number_t, string_t>{ "sequence_number", "code" },

        /// Miscellaneous.
        method<"reconnect", string_t, number_t>{ "new_host", "new_port" }
    };

    template <typename... Args>
    using subscriber = network::unsubscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using setup_connection = at<0>;
    using setup_connection_success = at<1>;
    using setup_connection_error = at<2>;
    using open_standard_mining_channel = at<3>;
    using open_standard_mining_channel_success = at<4>;
    using open_standard_mining_channel_error = at<5>;
    using open_extended_mining_channel = at<6>;
    using open_extended_mining_channel_success = at<7>;
    using open_extended_mining_channel_error = at<8>;
    using update_channel = at<9>;
    using update_channel_error = at<10>;
    using close_channel = at<11>;
    using set_target = at<12>;
    using new_mining_job = at<13>;
    using set_new_prev_hash = at<14>;
    using new_extended_mining_job = at<15>;
    using set_custom_mining_job = at<16>;
    using set_custom_mining_job_success = at<17>;
    using set_custom_mining_job_error = at<18>;
    using submit_shares = at<19>;
    using submit_shares_success = at<20>;
    using submit_shares_error = at<21>;
    using reconnect = at<22>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
