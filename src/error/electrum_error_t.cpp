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
#include <bitcoin/server/error/electrum_error_t.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace error {
namespace electrum {

DEFINE_ERROR_T_MESSAGE_MAP(error)
{
    // general
    { success, "success" },

    // application
    { bad_request, "bad_request" },
    { daemon_error, "daemon_error" },
    { excessive_history, "excessive_history" },

    // json-rpc
    { invalid_request, "invalid_request" },
    { method_not_found, "method_not_found" },
    { invalid_args, "invalid_args" },
    { internal_error, "internal_error" },
    { parse_error, "parse_error" },

    // resource
    { unavailable, "unavailable" },
    { excessive_resource_usage, "excessive_resource_usage" },
    { server_busy, "server_busy" }
};

DEFINE_ERROR_T_CATEGORY(error, "electrum", "electrum code")

code translate(const code& ec, error_t failure) NOEXCEPT
{
    if (!ec)
        return success;

    if (error_category::contains(ec))
        return ec;

    if (ec == database::error::depth_limited)
        return excessive_history;

    if (database::error::error_category::contains(ec))
        return daemon_error;

    return failure;
}

} // namespace electrum
} // namespace error
} // namespace server
} // namespace libbitcoin
