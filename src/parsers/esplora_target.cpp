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
#include <bitcoin/server/parsers/esplora_target.hpp>

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

static void set_media(network::rpc::object_t& params,
    network::http::media_type media) NOEXCEPT
{
    params["media"] = to_value(media);
}

code esplora_target(request_t& out, const std::string_view& path) NOEXCEPT
{
    constexpr auto text = network::http::media_type::text_plain;
    constexpr auto json = network::http::media_type::application_json;
    constexpr auto data = network::http::media_type::application_octet_stream;

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
    const auto target = segments[segment++];
    if (target.empty())
        return error::missing_target;

    if (target == "tx")
    {
        if (segment == segments.size())
        {
            set_media(params, text);
            method = "broadcast";
        }
        else
        {
            const auto hash = to_hash(segments[segment++]);
            if (!hash) return error::invalid_hash;

            params["hash"] = hash;
            if (segment == segments.size())
            {
                set_media(params, json);
                method = "tx";
            }
            else
            {
                const auto component = segments[segment++];
                if (component == "hex")
                {
                    set_media(params, text);
                    method = "tx";
                }
                else if (component == "raw")
                {
                    set_media(params, data);
                    method = "tx";
                }
                else if (component == "status")
                {
                    set_media(params, json);
                    method = "tx_status";
                }
                else if (component == "merkleblock-proof")
                {
                    set_media(params, text);
                    method = "tx_merkleblock_proof";
                }
                else if (component == "merkle-proof")
                {
                    set_media(params, json);
                    method = "tx_merkle_proof";
                }
                else if (component == "outspend")
                {
                    if (segment == segments.size())
                        return error::missing_position;

                    uint32_t index{};
                    if (!to_number(index, segments[segment++]))
                        return error::invalid_number;

                    params["index"] = index;
                    set_media(params, json);
                    method = "tx_outspend";
                }
                else if (component == "outspends")
                {
                    set_media(params, json);
                    method = "tx_outspends";
                }
                else
                {
                    return error::invalid_component;
                }
            }
        }
    }
    else if (target == "txs")
    {
        if (segment == segments.size())
            return error::missing_component;

        if (segments[segment++] != "package")
            return error::invalid_component;

        set_media(params, json);
        method = "broadcast_package";
    }
    else if (target == "address" || target == "scripthash")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto key = segments[segment++];
        if (target == "scripthash")
        {
            const auto hash = to_hash(key);
            if (!hash) return error::invalid_hash;

            params["hash"] = hash;
        }
        else
        {
            params["address"] = key;
        }

        if (segment == segments.size())
        {
            set_media(params, json);
            method = "address";
        }
        else
        {
            const auto component = segments[segment++];
            if (component == "txs")
            {
                if (segment == segments.size())
                {
                    set_media(params, json);
                    method = "address_txs";
                }
                else
                {
                    const auto subcomponent = segments[segment++];
                    if (subcomponent == "chain")
                    {
                        if (segment != segments.size())
                        {
                            const auto last = to_hash(segments[segment++]);
                            if (!last) return error::invalid_hash;

                            params["last_seen"] = last;
                        }

                        set_media(params, json);
                        method = "address_txs_chain";
                    }
                    else if (subcomponent == "mempool")
                    {
                        set_media(params, json);
                        method = "address_txs_mempool";
                    }
                    else
                    {
                        return error::invalid_subcomponent;
                    }
                }
            }
            else if (component == "utxo")
            {
                set_media(params, json);
                method = "address_utxo";
            }
            else
            {
                return error::invalid_component;
            }
        }
    }
    else if (target == "block")
    {
        if (segment == segments.size())
            return error::missing_hash;

        const auto hash = to_hash(segments[segment++]);
        if (!hash) return error::invalid_hash;

        params["hash"] = hash;
        if (segment == segments.size())
        {
            set_media(params, json);
            method = "block";
        }
        else
        {
            const auto component = segments[segment++];
            if (component == "raw")
            {
                set_media(params, data);
                method = "block";
            }
            else if (component == "header")
            {
                set_media(params, text);
                method = "block_header";
            }
            else if (component == "status")
            {
                set_media(params, json);
                method = "block_status";
            }
            else if (component == "txs")
            {
                if (segment != segments.size())
                {
                    uint32_t start{};
                    if (!to_number(start, segments[segment++]))
                        return error::invalid_number;

                    params["start"] = start;
                }

                set_media(params, json);
                method = "block_txs";
            }
            else if (component == "txids")
            {
                set_media(params, json);
                method = "block_txids";
            }
            else if (component == "txid")
            {
                if (segment == segments.size())
                    return error::missing_position;

                uint32_t index{};
                if (!to_number(index, segments[segment++]))
                    return error::invalid_number;

                params["index"] = index;
                set_media(params, text);
                method = "block_txid";
            }
            else
            {
                return error::invalid_component;
            }
        }
    }
    else if (target == "block-height")
    {
        if (segment == segments.size())
            return error::missing_height;

        uint32_t height{};
        if (!to_number(height, segments[segment++]))
            return error::invalid_number;

        params["height"] = height;
        set_media(params, text);
        method = "block_height";
    }
    else if (target == "blocks")
    {
        if (segment == segments.size())
        {
            set_media(params, json);
            method = "blocks";
        }
        else if (segments[segment] == "tip")
        {
            segment++;
            if (segment == segments.size())
                return error::missing_component;

            const auto subcomponent = segments[segment++];
            if (subcomponent == "height")
            {
                set_media(params, text);
                method = "tip_height";
            }
            else if (subcomponent == "hash")
            {
                set_media(params, text);
                method = "tip_hash";
            }
            else
            {
                return error::invalid_subcomponent;
            }
        }
        else
        {
            uint32_t height{};
            if (!to_number(height, segments[segment++]))
                return error::invalid_number;

            params["height"] = height;
            set_media(params, json);
            method = "blocks";
        }
    }
    else if (target == "mempool")
    {
        if (segment == segments.size())
        {
            set_media(params, json);
            method = "mempool";
        }
        else
        {
            const auto component = segments[segment++];
            if (component == "txids")
            {
                set_media(params, json);
                method = "mempool_txids";
            }
            else if (component == "recent")
            {
                set_media(params, json);
                method = "mempool_recent";
            }
            else
            {
                return error::invalid_component;
            }
        }
    }
    else if (target == "fee-estimates")
    {
        set_media(params, json);
        method = "fee_estimates";
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
