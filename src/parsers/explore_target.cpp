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
#include <bitcoin/server/parsers/explore_target.hpp>

#include <ranges>
#include <optional>
#include <variant>
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

static hash_cptr to_hash(const std::string_view& token) NOEXCEPT
{
    hash_digest out{};
    return decode_hash(out, token) ?
        emplace_shared<const hash_digest>(std::move(out)) : hash_cptr{};
}

code explore_target(request_t& out, const std::string_view& path) NOEXCEPT
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

    // transaction, address, inputs, and outputs are identical excluding names;
    // input and output are identical excluding names; block is unique.
    const auto target = segments[segment++];
    if (target == "top")
    {
        method = "top";
    }
    else if (target == "address")
    {
        if (segment == segments.size())
            return error::missing_hash;

        // address hash is a single sha256, in reversed display endianness.
        const auto base16 = to_hash(segments[segment++]);
        if (!base16) return error::invalid_hash;

        params["hash"] = base16;
        if (segment == segments.size())
        {
            method = "address";
        }
        else
        {
            const auto subcomponent = segments[segment++];
            if (subcomponent == "confirmed")
                method = "address_confirmed";
            else if (subcomponent == "unconfirmed")
                method = "address_unconfirmed";
            else if (subcomponent == "balance")
                method = "address_balance";
            else
                return error::invalid_subcomponent;
        }
    }
    else if (target == "input")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        params["hash"] = hash;
        if (segment == segments.size())
        {
            method = "inputs";
        }
        else
        {
            const auto component = segments[segment++];
            uint32_t index{};
            if (!to_number(index, component))
                return error::invalid_number;

            params["index"] = index;
            if (segment == segments.size())
            {
                method = "input";
            }
            else
            {
                const auto subcomponent = segments[segment++];
                if (subcomponent == "script")
                    method = "input_script";
                else if (subcomponent == "witness")
                    method = "input_witness";
                else
                    return error::invalid_subcomponent;
            }
        }
    }
    else if (target == "output")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        params["hash"] = hash;
        if (segment == segments.size())
        {
            method = "outputs";
        }
        else
        {
            const auto component = segments[segment++];
            uint32_t index{};
            if (!to_number(index, component))
                return error::invalid_number;

            params["index"] = index;
            if (segment == segments.size())
            {
                method = "output";
            }
            else
            {
                const auto subcomponent = segments[segment++];
                if (subcomponent == "script")
                    method = "output_script";
                else if (subcomponent == "spender")
                    method = "output_spender";
                else if (subcomponent == "spenders")
                    method = "output_spenders";
                else
                    return error::invalid_subcomponent;
            }
        }
    }
    else if (target == "tx")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        params["hash"] = hash;
        if (segment == segments.size())
        {
            method = "tx";
        }
        else
        {
            const auto component = segments[segment++];
            if (component == "header")
                method = "tx_header";
            else if (component == "details")
                method = "tx_details";
            else
                return error::invalid_component;
        }
    }
    else if (target == "block")
    {
        if (segment == segments.size())
            return error::missing_id_type;

        const auto by = segments[segment++];
        if (by == "hash")
        {
            if (segment == segments.size())
                return error::missing_hash;

            const auto hash = to_hash(segments[segment++]);
            if (!hash) return error::invalid_hash;

            params["hash"] = hash;
        }
        else if (by == "height")
        {
            if (segment == segments.size())
                return error::missing_height;

            uint32_t height{};
            if (!to_number(height, segments[segment++]))
                return error::invalid_number;

            params["height"] = height;
        }
        else
        {
            return error::invalid_id_type;
        }

        if (segment == segments.size())
        {
            method = "block";
        }
        else
        {
            const auto component = segments[segment++];
            if (component == "tx")
            {
                if (segment == segments.size())
                    return error::missing_position;

                uint32_t position{};
                if (!to_number(position, segments[segment++]))
                    return error::invalid_number;

                params["position"] = position;
                method = "block_tx";
            }
            else if (component == "header")
            {
                if (segment == segments.size())
                {
                    method = "block_header";
                }
                else
                {
                    const auto subcomponent = segments[segment++];
                    if (subcomponent == "context")
                        method = "block_header_context";
                    else
                        return error::invalid_subcomponent;
                }
            }
            else if (component == "txs")
                method = "block_txs";
            else if (component == "details")
                method = "block_details";
            else if (component == "filter")
            {
                if (segment == segments.size())
                    return error::missing_type_id;

                uint8_t type{};
                if (!to_number(type, segments[segment++]))
                    return error::invalid_number;

                params["type"] = type;
                if (segment == segments.size())
                {
                    method = "block_filter";
                }
                else
                {
                    const auto subcomponent = segments[segment++];
                    if (subcomponent == "hash")
                        method = "block_filter_hash";
                    else if (subcomponent == "header")
                        method = "block_filter_header";
                    else
                        return error::invalid_subcomponent;
                }
            }
            else
                return error::invalid_component;
        }
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
