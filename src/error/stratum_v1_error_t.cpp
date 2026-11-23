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
#include <bitcoin/server/error/stratum_v1_error_t.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace error {
namespace stratum_v1 {

DEFINE_ERROR_T_MESSAGE_MAP(error)
{
    // general
    { success, "success" },

    // share
    { unknown, "Other/Unknown" },
    { job_not_found, "Job not found" },
    { duplicate_share, "Duplicate share" },
    { low_difficulty_share, "Low difficulty share" },

    // worker
    { unauthorized_worker, "Unauthorized worker" },
    { not_subscribed, "Not subscribed" }
};

DEFINE_ERROR_T_CATEGORY(error, "stratum_v1", "stratum v1 code")

} // namespace stratum_v1
} // namespace error
} // namespace server
} // namespace libbitcoin
