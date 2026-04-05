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
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

bool protocol_native::handle_get_inputs(const code& ec, interface::inputs,
    uint8_t, uint8_t media, const hash_cptr& hash, bool witness) NOEXCEPT
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

    const auto inputs = query.get_inputs(tx, witness);
    if (!inputs || inputs->empty())
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    // Wire serialization of input does not include witness.
    const auto size = std::accumulate(inputs->begin(), inputs->end(), zero,
        [&](size_t total, const auto& input) NOEXCEPT
        { return total + input->serialized_size(false); });

    switch (media)
    {
        case data:
            send_chunk(to_bin_ptr_array(*inputs, size));
            return true;
        case text:
            send_text(to_hex_ptr_array(*inputs, size));
            return true;
        case json:
            // Json input serialization includes witness.
            send_json(value_from(*inputs), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_input(const code& ec, interface::input,
    uint8_t, uint8_t media, const hash_cptr& hash, uint32_t index,
    bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto input = query.get_input(query.to_tx(*hash), index, witness))
    {
        // Wire serialization of input does not include witness.
        const auto size = input->serialized_size(false);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*input, size));
                return true;
            case text:
                send_text(to_hex(*input, size));
                return true;
            case json:
                // Json input serialization includes witness.
                send_json(value_from(input),
                    two * input->serialized_size(witness));
            return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_input_script(const code& ec,
    interface::input_script, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto script = query.get_input_script(query.to_point(
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

bool protocol_native::handle_get_input_witness(const code& ec,
    interface::input_witness, uint8_t, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto witness = query.get_witness(query.to_point(
        query.to_tx(*hash), index)); !is_null(witness) && witness->is_valid())
    {
        const auto size = witness->serialized_size(false);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*witness, size, false));
                return true;
            case text:
                send_text(to_hex(*witness, size, false));
                return true;
            case json:
                send_json(value_from(witness),
                    two * witness->serialized_size(false));
            return true;
        }
    }

    send_not_found();
    return true;
}

} // namespace server
} // namespace libbitcoin
