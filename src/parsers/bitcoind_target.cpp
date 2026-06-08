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
#include <bitcoin/server/parsers/bitcoind_target.hpp>

#include <ranges>
#include <optional>
#include <variant>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;
using namespace network::http;

BC_PUSH_WARNING(NO_ARRAY_INDEXING)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

static hash_cptr to_hash(const std::string_view& token) NOEXCEPT
{
    hash_digest out{};
    return decode_hash(out, token) ?
        emplace_shared<const hash_digest>(std::move(out)) : hash_cptr{};
}

// Map a Bitcoin Core REST file extension to a media value.
static bool to_media(uint8_t& out, const std::string_view& extension) NOEXCEPT
{
    if (extension == "bin")
    {
        out = to_value(media_type::application_octet_stream);
        return true;
    }
    if (extension == "hex")
    {
        out = to_value(media_type::text_plain);
        return true;
    }
    if (extension == "json")
    {
        out = to_value(media_type::application_json);
        return true;
    }

    return false;
}

// Parse a Bitcoin Core REST path into a json-rpc request model.
// github.com/bitcoin/bitcoin/blob/master/doc/REST-interface.md
// Currently supports: /rest/block/<hash>.<bin|hex|json>
code bitcoind_target(request_t& out, const std::string_view& path) NOEXCEPT
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

    // Accept an optional "rest" prefix (Core mounts endpoints under /rest/).
    if (segments[segment] == "rest")
        ++segment;

    if (segment == segments.size())
        return error::missing_target;

    const auto target = segments[segment++];
    if (target == "block")
    {
        if (segment == segments.size())
            return error::missing_hash;

        // Final segment is "<hash>.<extension>".
        const auto leaf = split(segments[segment++], ".", false, true);
        if (leaf.size() != two)
            return error::invalid_target;

        const auto hash = to_hash(leaf.front());
        if (!hash)
            return error::invalid_hash;

        uint8_t media{};
        if (!to_media(media, leaf.back()))
            return error::invalid_target;

        method = "block";
        params["media"] = media;
        params["hash"] = hash;
        return error::success;
    }

    return error::invalid_target;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
