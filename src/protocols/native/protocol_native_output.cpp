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
#include <optional>
#include <string>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

bool protocol_native::handle_get_outputs(const code& ec, interface::outputs,
    uint8_t, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto tx = query.to_tx(*hash);
    if (tx.is_terminal())
    {
        send_not_found();
        return true;
    }

    const auto outputs = query.get_outputs(tx);
    if (!outputs || outputs->empty())
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    // Wire serialization size of outputs set.
    const auto size = std::accumulate(outputs->begin(), outputs->end(), zero,
        [](size_t total, const auto& output) NOEXCEPT
        { return total + output->serialized_size(); });

    switch (media)
    {
        case data:
            send_chunk(to_bin_ptr_array(*outputs, size));
            return true;
        case text:
            send_text(to_hex_ptr_array(*outputs, size));
            return true;
        case json:
            send_json(value_from(*outputs), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_output(const code& ec, interface::output,
    uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto output = query.get_output(query.to_tx(*hash), index))
    {
        const auto size = output->serialized_size();
        switch (media)
        {
            case data:
                send_chunk(to_bin(*output, size));
                return true;
            case text:
                send_text(to_hex(*output, size));
                return true;
            case json:
                send_json(value_from(output), two * size);
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_output_script(const code& ec,
    interface::output_script, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto script = query.get_output_script(query.to_output(
        query.to_tx(*hash), index)))
    {
        const auto size = script->serialized_size(false);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*script, size, false));
                return true;
            case text:
                send_text(to_hex(*script, size, false));
                return true;
            case json:
                send_json(value_from(script), two * size);
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_output_spender(const code& ec,
    interface::output_spender, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const chain::point spent{ *hash, index };
    const auto spender = query.get_spender(query.find_confirmed_spender(spent));
    if (spender.is_null())
    {
        send_not_found();
        return true;
    }

    constexpr auto size = chain::point::serialized_size();
    switch (media)
    {
        case data:
            send_chunk(to_bin(spender, size));
            return true;
        case text:
            send_text(to_hex(spender, size));
            return true;
        case json:
            send_json(value_from(spender), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_output_spenders(const code& ec,
    interface::output_spenders, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto ins = archive().get_spenders({ *hash, index });
    if (ins.empty())
    {
        send_not_found();
        return true;
    }

    const auto size = ins.size() * database::inpoint::serialized_size();
    switch (media)
    {
        case data:
            send_chunk(to_bin_array(ins, size));
            return true;
        case text:
            send_text(to_hex_array(ins, size));
            return true;
        case json:
            send_json(value_from(ins), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_output_subscribe(const code& ec,
    interface::output_subscribe, uint8_t version, uint8_t media,
    const system::hash_cptr& hash, uint32_t index) NOEXCEPT
{
    // TODO
    return {};
}

// OP_RETURN helpers.
// ----------------------------------------------------------------------------
// private/static

static bool starts_with_bytes(const data_chunk& payload,
    const uint8_t* prefix, size_t prefix_size) NOEXCEPT
{
    return payload.size() >= prefix_size &&
        std::equal(prefix, prefix + prefix_size, payload.begin());
}

static const char* detect_op_return_protocol(
    const chain::operations& ops, const data_chunk& payload) NOEXCEPT
{
    // Runes: OP_RETURN OP_13 <data> — identified by opcode, not payload bytes.
    if (ops.size() >= 2 && ops[1].code() == chain::opcode::push_positive_13)
        return "Runes";

    static constexpr uint8_t omni[]         = { 0x6f, 0x6d, 0x6e, 0x69 };
    static constexpr uint8_t counterparty[] = { 0x43, 0x4e, 0x54, 0x52, 0x50 };
    static constexpr uint8_t stamps[]       = { 0x53, 0x54, 0x4d, 0x50 };
    static constexpr uint8_t bip320[]       = { 0x42, 0x49, 0x50, 0x33, 0x32, 0x30 };

    if (starts_with_bytes(payload, omni, sizeof(omni)))
        return "Omni Layer";
    if (starts_with_bytes(payload, counterparty, sizeof(counterparty)))
        return "Counterparty";
    if (starts_with_bytes(payload, stamps, sizeof(stamps)))
        return "Stamps";
    if (starts_with_bytes(payload, bip320, sizeof(bip320)))
        return "BIP-320";
    return nullptr;
}

static std::optional<std::string> try_utf8(
    const data_chunk& payload) NOEXCEPT
{
    size_t i = 0;
    while (i < payload.size())
    {
        const auto byte = static_cast<uint8_t>(payload[i]);
        size_t extra;
        if ((byte & 0x80) == 0x00) extra = 0;
        else if ((byte & 0xe0) == 0xc0) extra = 1;
        else if ((byte & 0xf0) == 0xe0) extra = 2;
        else if ((byte & 0xf8) == 0xf0) extra = 3;
        else return std::nullopt;

        if (i + extra >= payload.size() && extra > 0) return std::nullopt;
        for (size_t j = 1; j <= extra; ++j)
            if ((static_cast<uint8_t>(payload[i + j]) & 0xc0) != 0x80)
                return std::nullopt;
        i += 1 + extra;
    }
    return std::string(payload.begin(), payload.end());
}

// Handler.
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_output_op_return(const code& ec,
    interface::output_op_return, uint8_t, uint8_t media,
    const hash_cptr& hash, uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto script = query.get_output_script(
        query.to_output(query.to_tx(*hash), index));

    if (!script)
    {
        send_not_found();
        return true;
    }

    const auto& ops = script->ops();
    if (ops.empty() || ops[0].code() != chain::opcode::op_return)
    {
        send_not_found();
        return true;
    }

    // Collect payload from all pushdata ops after OP_RETURN.
    data_chunk payload{};
    for (size_t i = 1; i < ops.size(); ++i)
    {
        const auto& bytes = ops[i].data();
        payload.insert(payload.end(), bytes.begin(), bytes.end());
    }

    if (payload.empty())
    {
        send_not_found();
        return true;
    }

    switch (media)
    {
        case data:
            send_chunk(data_chunk(payload.begin(), payload.end()));
            return true;
        case text:
            send_text(encode_base16(payload));
            return true;
        case json:
        {
            boost::json::object object;
            object.emplace("hex",    encode_base16(payload));
            object.emplace("base64", encode_base64(payload));
            object.emplace("size",   payload.size());

            const auto* protocol = detect_op_return_protocol(ops, payload);
            if (protocol != nullptr)
                object.emplace("protocol", protocol);

            const auto utf8 = try_utf8(payload);
            if (utf8.has_value())
                object.emplace("utf8", utf8.value());

            send_json(std::move(object), two * payload.size() + 64);
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
