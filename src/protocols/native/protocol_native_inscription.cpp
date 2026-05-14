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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::http;

BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Inscription parsing helpers.
// ----------------------------------------------------------------------------
// private/static

// Inscription mark bytes: OP_FALSE OP_IF OP_PUSHBYTES_3 "ord"
// Matches ordpool-parser's INSCRIPTION_MARK constant.
// https://github.com/ordpool-space/ordpool-parser
static constexpr uint8_t inscription_mark[] =
    { 0x00, 0x63, 0x03, 0x6f, 0x72, 0x64 };
static constexpr size_t inscription_mark_size = 6;

// Field tags per the Ordinals protocol.
static constexpr uint8_t field_content_type     = 0x01;
static constexpr uint8_t field_content_encoding = 0x09;

// OP_0 / separator and OP_ENDIF byte values.
static constexpr uint8_t op_0    = 0x00;
static constexpr uint8_t op_endif = 0x68;

struct inscription_t
{
    std::string content_type;
    std::string content_encoding; // optional, e.g. "br" or "gzip"
    data_chunk body;
};

// Read one pushdata operation at position p; advances p past the push.
// Returns data bytes for the push, or nullopt for non-pushdata opcodes.
static std::optional<data_chunk> read_pushdata(const data_chunk& raw,
    size_t& p) NOEXCEPT
{
    if (p >= raw.size())
        return std::nullopt;

    const auto op = static_cast<size_t>(raw[p++]);

    // push_size_0..push_size_75: opcode value is the byte count.
    if (op <= 0x4b)
    {
        if (p + op > raw.size()) return std::nullopt;
        data_chunk out(raw.begin() + p, raw.begin() + p + op);
        p += op;
        return out;
    }

    // OP_PUSHDATA1 (0x4c): next byte is size.
    if (op == 0x4c)
    {
        if (p >= raw.size()) return std::nullopt;
        const size_t size = raw[p++];
        if (p + size > raw.size()) return std::nullopt;
        data_chunk out(raw.begin() + p, raw.begin() + p + size);
        p += size;
        return out;
    }

    // OP_PUSHDATA2 (0x4d): next 2 bytes LE is size.
    if (op == 0x4d)
    {
        if (p + 2 > raw.size()) return std::nullopt;
        const size_t size = raw[p] | (static_cast<size_t>(raw[p + 1]) << 8);
        p += 2;
        if (p + size > raw.size()) return std::nullopt;
        data_chunk out(raw.begin() + p, raw.begin() + p + size);
        p += size;
        return out;
    }

    // OP_PUSHDATA4 (0x4e): next 4 bytes LE is size.
    if (op == 0x4e)
    {
        if (p + 4 > raw.size()) return std::nullopt;
        const size_t size =
            raw[p] |
            (static_cast<size_t>(raw[p + 1]) << 8) |
            (static_cast<size_t>(raw[p + 2]) << 16) |
            (static_cast<size_t>(raw[p + 3]) << 24);
        p += 4;
        if (p + size > raw.size()) return std::nullopt;
        data_chunk out(raw.begin() + p, raw.begin() + p + size);
        p += size;
        return out;
    }

    // Non-pushdata opcode: back up and return nullopt.
    --p;
    return std::nullopt;
}

// Search raw script bytes for the inscription mark.
// Returns the offset of the mark, or raw.size() if not found.
static size_t find_inscription_mark(const data_chunk& raw) NOEXCEPT
{
    if (raw.size() < inscription_mark_size)
        return raw.size();

    for (size_t i = 0; i <= raw.size() - inscription_mark_size; ++i)
    {
        if (std::memcmp(raw.data() + i, inscription_mark,
            inscription_mark_size) == 0)
            return i;
    }
    return raw.size();
}

static std::optional<inscription_t> parse_inscription(
    const data_chunk& script_bytes) NOEXCEPT
{
    const auto mark = find_inscription_mark(script_bytes);
    if (mark >= script_bytes.size())
        return std::nullopt;

    // Advance past OP_FALSE OP_IF OP_PUSHBYTES_3 "ord".
    size_t p = mark + inscription_mark_size;

    inscription_t result;

    // Parse field-value pairs until OP_0 separator or OP_ENDIF.
    while (p < script_bytes.size())
    {
        const auto peek = script_bytes[p];
        if (peek == op_0)    { ++p; break; }  // separator: body follows
        if (peek == op_endif) return result;   // no body

        const auto tag = read_pushdata(script_bytes, p);
        if (!tag.has_value()) return result;

        const auto val = read_pushdata(script_bytes, p);
        if (!val.has_value()) return result;

        if (tag->size() == 1)
        {
            switch ((*tag)[0])
            {
                case field_content_type:
                    result.content_type =
                        std::string(val->begin(), val->end());
                    break;
                case field_content_encoding:
                    result.content_encoding =
                        std::string(val->begin(), val->end());
                    break;
                default:
                    break;
            }
        }
    }

    // Collect body chunks until OP_ENDIF.
    while (p < script_bytes.size())
    {
        if (script_bytes[p] == op_endif) break;

        const auto chunk = read_pushdata(script_bytes, p);
        if (!chunk.has_value()) break;
        result.body.insert(result.body.end(), chunk->begin(), chunk->end());
    }

    if (result.content_type.empty() && result.body.empty())
        return std::nullopt;

    return result;
}

// Extract the tapscript leaf from a taproot input witness (BIP-341).
// Returns nullptr if the witness doesn't look like a tapscript spend.
static const data_chunk* tapscript_from_witness(
    const chain::witness& witness) NOEXCEPT
{
    const auto& stack = witness.stack();
    if (stack.size() < 2)
        return nullptr;

    const bool has_annex = chain::annex::is_annex_pattern(stack);

    const size_t effective = stack.size() - (has_annex ? 1 : 0);
    if (effective < 2)
        return nullptr;

    // Tapscript is second-to-last (effective index = effective - 2).
    return stack[effective - 2].get();
}

// Handler.
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_inscription(const code& ec,
    interface::inscription, uint8_t, uint8_t media,
    const hash_cptr& hash, uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto witness = query.get_witness(
        query.to_point(query.to_tx(*hash), index));

    if (is_null(witness) || !witness->is_valid())
    {
        send_not_found();
        return true;
    }

    const auto* script_bytes = tapscript_from_witness(*witness);
    if (script_bytes == nullptr)
    {
        send_not_found();
        return true;
    }

    const auto inscription = parse_inscription(*script_bytes);
    if (!inscription.has_value())
    {
        send_not_found();
        return true;
    }

    switch (media)
    {
        case data:
        {
            // Serve raw content bytes with the inscription's content-type.
            const auto type = content_media_type(inscription->content_type,
                media_type::application_octet_stream);
            send_typed_chunk(data_chunk{ inscription->body }, type);
            return true;
        }
        case text:
            send_text(encode_base16(inscription->body));
            return true;
        case json:
        {
            boost::json::object object;
            object.emplace("content_type",   inscription->content_type);
            object.emplace("content_base64", encode_base64(inscription->body));
            object.emplace("size",           inscription->body.size());
            if (!inscription->content_encoding.empty())
                object.emplace("content_encoding", inscription->content_encoding);

            send_json(std::move(object), two * inscription->body.size() + 128);
            return true;
        }
    }

    send_not_found();
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
