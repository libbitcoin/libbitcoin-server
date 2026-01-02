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
#include <bitcoin/server/parsers/explore_query.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::http;

BC_PUSH_WARNING(NO_ARRAY_INDEXING)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

bool explore_query(rpc::request_t& out, const request& request) NOEXCEPT
{
    wallet::uri uri{};
    if (!uri.decode(request.target()))
        return false;

    constexpr auto text = media_type::text_plain;
    constexpr auto json = media_type::application_json;
    constexpr auto data = media_type::application_octet_stream;

    auto query = uri.decode_query();
    const auto format = query["format"];

    if (!out.params.has_value() ||
        !std::holds_alternative<rpc::object_t>(out.params.value()))
        return false;

    auto& params = std::get<rpc::object_t>(out.params.value());

    // Validate proper witness bool value if set.
    const auto witness = query.find("witness");
    if (witness != query.end())
    {
        if (witness->second != "true" && witness->second != "false")
            return false;

        // Witness is optional<true> (where applicable), so only set if false.
        if (witness->second == "false")
            params["witness"] = false;
    }

    // Validate proper turbo bool value if set.
    const auto turbo = query.find("turbo");
    if (turbo != query.end())
    {
        if (turbo->second != "true" && turbo->second != "false")
            return false;

        // Turbo is optional<true> (where applicable), so only set if false.
        if (turbo->second == "false")
            params["turbo"] = false;
    }

    const auto accepts = to_media_types((request)[field::accept]);

    if (contains(accepts, text) || format == "text")
    {
        params["media"] = to_value(text);
        return true;
    }

    if (contains(accepts, data) || format == "data")
    {
        params["media"] = to_value(data);
        return true;
    }

    ////// Default format to json.
    ////params["media"] = to_value(json);
    ////return true;

    if (contains(accepts, json) || format == "json")
    {
        params["media"] = to_value(json);
        return true;
    }

    return false;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
