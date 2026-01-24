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
#ifndef LIBBITCOIN_SERVER_PARSERS_EXPLORE_QUERY_HPP
#define LIBBITCOIN_SERVER_PARSERS_EXPLORE_QUERY_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

// Query string tokens.
namespace token
{
    // Names.
    constexpr auto turbo = "turbo";
    constexpr auto format = "format";
    constexpr auto witness = "witness";

    // Boolean values.
    constexpr auto true_ = "true";
    constexpr auto false_ = "false";

    // Format values.
    namespace formats
    {
        constexpr auto html = "html";
        constexpr auto text = "text";
        constexpr auto json = "json";
        constexpr auto data = "data";
    }
}

BCS_API bool explore_query(network::rpc::request_t& out,
    const network::http::request& request) NOEXCEPT;

BCS_API network::http::media_type get_media(
    const network::rpc::request_t& model) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
