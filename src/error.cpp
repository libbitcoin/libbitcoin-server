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
#include <bitcoin/server/error.hpp>

#include <bitcoin/system.hpp>

namespace libbitcoin {
namespace server {
namespace error {

DEFINE_ERROR_T_MESSAGE_MAP(error)
{
    // general
    { success, "success" },

    // server (url parse codes)
    { empty_path, "empty_path" },
    { invalid_number, "invalid_number" },
    { invalid_hash, "invalid_hash" },
    { missing_version, "missing_version" },
    { missing_target, "missing_target" },
    { invalid_target, "invalid_target" },
    { missing_hash, "missing_hash" },
    { missing_height, "missing_height" },
    { missing_position, "missing_position" },
    { missing_id_type, "missing_id_type" },
    { invalid_id_type, "invalid_id_type" },
    { missing_type_id, "missing_type_id" },
    { missing_component, "missing_component" },
    { invalid_component, "invalid_component" },
    { invalid_subcomponent, "invalid_subcomponent" },
    { extra_segment, "extra_segment" },

    // server (rpc response codes)
    { not_found, "not_found" },
    { invalid_argument, "invalid_argument" },
    { not_implemented, "not_implemented" }
};

DEFINE_ERROR_T_CATEGORY(error, "server", "server code")

} // namespace error
} // namespace server
} // namespace libbitcoin
