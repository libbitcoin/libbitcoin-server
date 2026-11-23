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
#ifndef LIBBITCOIN_SERVER_ERROR_STRATUM_V1_ERROR_T_HPP
#define LIBBITCOIN_SERVER_ERROR_STRATUM_V1_ERROR_T_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/server/error/error_t.hpp>

namespace libbitcoin {
namespace server {
namespace error {
namespace stratum_v1 {

/// Values are stratum v1 wire codes, published as json-rpc error code.
enum error_t : int32_t
{
    /// general
    success = 0,

    /// share
    unknown = 20,
    job_not_found = 21,
    duplicate_share = 22,
    low_difficulty_share = 23,

    /// worker
    unauthorized_worker = 24,
    not_subscribed = 25
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error, BCS_API);

} // namespace stratum_v1
} // namespace error
} // namespace server
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::server::error::stratum_v1::error)

#endif
