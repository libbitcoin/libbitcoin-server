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
#ifndef LIBBITCOIN_SERVER_ERROR_HPP
#define LIBBITCOIN_SERVER_ERROR_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/server/version.hpp>

namespace libbitcoin {
namespace server {

/// Alias system code.
/// std::error_code "server" category holds server::error::error_t.
typedef std::error_code code;

namespace error {

/// Asio failures are normalized to the error codes below.
/// Stop by explicit call is mapped to channel_stopped or service_stopped
/// depending on the context. Asio errors returned on cancel calls are ignored.
enum error_t : uint8_t
{
    /// general
    success,

    /// server (url parse codes)
    empty_path,
    invalid_number,
    invalid_hash,
    missing_version,
    missing_target,
    invalid_target,
    missing_hash,
    missing_height,
    missing_position,
    missing_id_type,
    invalid_id_type,
    missing_type_id,
    missing_component,
    invalid_component,
    invalid_subcomponent,
    extra_segment,

    /// server (rpc response codes)
    not_found,
    invalid_argument,
    not_implemented
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

} // namespace error
} // namespace server
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::server::error::error)

#endif
