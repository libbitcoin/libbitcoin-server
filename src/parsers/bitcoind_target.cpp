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

#include <charconv>
#include <iterator>
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
        to_shared(std::move(out)) : hash_cptr{};
}

// Map a bitcoind REST file extension to a media value.
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

template <typename Number>
static bool to_number(Number& out, const std::string_view& token) NOEXCEPT
{
    if (token.empty() || (token.size() > one && token.starts_with('0')))
        return false;

    const auto end = std::next(token.data(), token.size());
    const auto result = std::from_chars(token.data(), end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

// Split a "<name>.<extension>" leaf into its name and a media value.
static bool split_leaf(std::string& name, uint8_t& media,
    const std::string_view& leaf) NOEXCEPT
{
    const auto parts = split(leaf, ".", false, true);
    if (parts.size() != two)
        return false;

    name = parts.front();
    return to_media(media, parts.back());
}

// Parse a bitcoind REST path into a json-rpc request model.
// github.com/bitcoin/bitcoin/blob/master/doc/REST-interface.md
// Supports: block, block/notxdetails, blockhashbyheight, headers, blockpart,
// chaininfo (remaining endpoints return invalid_target until implemented).
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

    // Accept an optional "rest" prefix (bitcoind mounts endpoints under /rest/).
    if (segments[segment] == "rest")
        ++segment;

    if (segment == segments.size())
        return error::missing_target;

    const auto target = segments[segment++];

    // /rest/chaininfo.json
    if (target == "chaininfo" || target == "chaininfo.json")
    {
        method = "chain_information";
        return error::success;
    }

    // /rest/block/<hash>.<ext>, /rest/block/notxdetails/<hash>.<ext> and
    // /rest/block/spent/<hash>.<ext> (the latter is a libbitcoin extension).
    if (target == "block")
    {
        if (segment == segments.size())
            return error::missing_hash;

        std::string rest_method = "block";
        if (segments[segment] == "notxdetails")
            rest_method = "block_txs";
        else if (segments[segment] == "spent")
            rest_method = "block_spent_tx_outputs";

        if (rest_method != "block" && ++segment == segments.size())
            return error::missing_hash;

        std::string name{};
        uint8_t media{};
        if (!split_leaf(name, media, segments[segment++]))
            return error::invalid_target;

        const auto hash = to_hash(name);
        if (!hash)
            return error::invalid_hash;

        method = rest_method;
        params["media"] = media;
        params["hash"] = hash;
        return error::success;
    }

    // /rest/blockhashbyheight/<height>.<ext>
    if (target == "blockhashbyheight")
    {
        if (segment == segments.size())
            return error::missing_target;

        std::string name{};
        uint8_t media{};
        if (!split_leaf(name, media, segments[segment++]))
            return error::invalid_target;

        uint32_t height{};
        if (!to_number(height, name))
            return error::invalid_number;

        method = "block_hash";
        params["media"] = media;
        params["height"] = height;
        return error::success;
    }

    // /rest/headers/<count>/<hash>.<ext>
    if (target == "headers")
    {
        if (segment == segments.size())
            return error::missing_target;

        uint32_t count{};
        if (!to_number(count, segments[segment++]))
            return error::invalid_number;

        if (segment == segments.size())
            return error::missing_hash;

        std::string name{};
        uint8_t media{};
        if (!split_leaf(name, media, segments[segment++]))
            return error::invalid_target;

        const auto hash = to_hash(name);
        if (!hash)
            return error::invalid_hash;

        method = "block_headers";
        params["media"] = media;
        params["hash"] = hash;
        params["count"] = count;
        return error::success;
    }

    // /rest/blockfilter/<type>/<hash>.<ext> and blockfilterheaders likewise.
    if (target == "blockfilter" || target == "blockfilterheaders")
    {
        if (segment == segments.size())
            return error::missing_target;

        // libbitcoin supports only the "basic" (neutrino) filter type.
        if (segments[segment++] != "basic")
            return error::invalid_target;

        if (segment == segments.size())
            return error::missing_hash;

        std::string name{};
        uint8_t media{};
        if (!split_leaf(name, media, segments[segment++]))
            return error::invalid_target;

        const auto hash = to_hash(name);
        if (!hash)
            return error::invalid_hash;

        method = target == "blockfilter" ? "block_filter" :
            "block_filter_headers";
        params["media"] = media;
        params["hash"] = hash;
        params["type"] = 0_u8;
        return error::success;
    }

    // /rest/blockpart/<hash>/<offset>/<size>.<ext> (libbitcoin extension)
    if (target == "blockpart")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash)
            return error::invalid_hash;

        if (segment == segments.size())
            return error::missing_target;

        uint32_t offset{};
        if (!to_number(offset, segments[segment++]))
            return error::invalid_number;

        if (segment == segments.size())
            return error::missing_target;

        std::string name{};
        uint8_t media{};
        if (!split_leaf(name, media, segments[segment++]))
            return error::invalid_target;

        uint32_t size{};
        if (!to_number(size, name))
            return error::invalid_number;

        method = "block_part";
        params["media"] = media;
        params["hash"] = hash;
        params["offset"] = offset;
        params["size"] = size;
        return error::success;
    }

    return error::invalid_target;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
