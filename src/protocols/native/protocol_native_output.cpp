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

} // namespace server
} // namespace libbitcoin
