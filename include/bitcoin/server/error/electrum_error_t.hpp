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
#ifndef LIBBITCOIN_SERVER_ERROR_ELECTRUM_ERROR_T_HPP
#define LIBBITCOIN_SERVER_ERROR_ELECTRUM_ERROR_T_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/server/error/error_t.hpp>

namespace libbitcoin {
namespace server {
namespace error {
namespace electrum {

/// Values are electrumx/aiorpcx wire codes, published as the json-rpc error
/// code. The electrum client disconnects on method_not_found, internal_error,
/// excessive_resource_usage and server_busy.
enum error_t : int32_t
{
    /// general
    success = 0,

    /// application (electrumx)
    bad_request = 1,
    daemon_error = 2,
    excessive_history = 10'001,

    /// json-rpc (aiorpcx)
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_args = -32602,
    internal_error = -32603,
    parse_error = -32700,

    /// resource (aiorpcx)
    unavailable = -100,
    excessive_resource_usage = -101,
    server_busy = -102
};

// No current need for error_code equivalence mapping.
DECLARE_ERROR_T_CODE_CATEGORY(error);

/// Map a foreign category code to the electrum code space, where a depth
/// limit is excessive history, any other store fault is a daemon error, and
/// any other failure is reported as the given code. A code of this category
/// passes through unchanged.
BC_API code translate(const code& ec, error_t failure) NOEXCEPT;

} // namespace electrum
} // namespace error
} // namespace server
} // namespace libbitcoin

DECLARE_STD_ERROR_REGISTRATION(bc::server::error::electrum::error)

#endif
