/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/parsers/admin_target.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_ARRAY_INDEXING)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

template <typename Number>
static bool to_number(Number& out, const std::string_view& token) NOEXCEPT
{
    return !token.empty() && is_ascii_numeric(token) && (is_one(token.size()) ||
        token.front() != '0') && deserialize(out, token);
}

code admin_target(request_t& out, const std::string_view& path) NOEXCEPT
{
    const auto clean = split(path, "?", false, false).front();
    if (clean.empty())
        return error::empty_path;

    // Avoid conflict with node type.
    using object_t = network::rpc::object_t;

    // Initialize json-rpc.v2 named params message.
    out = request_t
    {
        .jsonrpc = version::v2,
        .id = null_t{},
        .method = {},
        .params = object_t{}
    };

    auto& method = out.method;
    auto& params = std::get<object_t>(out.params.value());
    const auto segments = split(clean, "/", false, true);
    BC_ASSERT(!segments.empty());

    size_t segment{};
    if (!segments[segment].starts_with('v'))
        return error::missing_version;

    uint8_t version{};
    if (!to_number(version, segments[segment++].substr(one)))
        return error::invalid_number;

    params["version"] = version;
    if (segment == segments.size())
        return error::missing_target;

    const auto target = segments[segment++];
    if (target == "log")
    {
        if (segment == segments.size())
            return error::invalid_subcomponent;

        if (segments[segment++] == "subscribe")
            method = "log_subscribe";
        else
            return error::invalid_subcomponent;
    }
    else if (target == "event")
    {
        if (segment == segments.size())
            return error::invalid_subcomponent;

        if (segments[segment++] == "subscribe")
            method = "event_subscribe";
        else
            return error::invalid_subcomponent;
    }
    else
    {
        return error::invalid_target;
    }

    return segment == segments.size() ? error::success : error::extra_segment;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
