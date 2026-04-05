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

#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

bool protocol_native::handle_get_tx(const code& ec, interface::tx, uint8_t,
    uint8_t media, const hash_cptr& hash, bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto tx = query.get_transaction(query.to_tx(*hash), witness))
    {
        const auto size = tx->serialized_size(witness);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*tx, size, witness));
                return true;
            case text:
                send_text(to_hex(*tx, size, witness));
                return true;
            case json:
                send_json(value_from(tx), two * size);
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_tx_header(const code& ec,
    interface::tx_header, uint8_t, uint8_t media,
    const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = query.find_confirmed_block(*hash);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    constexpr auto size = chain::header::serialized_size();
    switch (media)
    {
        case data:
            send_chunk(to_bin(*header, size));
            return true;
        case text:
            send_text(to_hex(*header, size));
            return true;
        case json:
            auto model = value_from(header);
            inject(model, {}, link);
            send_json(std::move(model), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_tx_details(const code& ec,
    interface::tx_details, uint8_t, uint8_t media,
    const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_tx(*hash);

    // Missing tx.
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    uint64_t value{}, spend{};
    size_t nominal{}, maximal{};
    const auto coinbase = query.is_coinbase(link);
    if (!query.get_tx_sizes(nominal, maximal, link) ||
        !query.get_tx_value(value, link) ||
        !query.get_tx_spend(spend, link) ||
        (!coinbase && is_subtract_overflow(value, spend)))
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    // sigops and wtxid are not cached, so removed for now.
    boost::json::object object
    {
        { "segregated", maximal != nominal },
        { "coinbase", coinbase },
        { "nominal", nominal },
        { "maximal", maximal },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "virtual", chain::virtual_size(nominal, maximal) },
        { "value", value },
        { "spend", spend },
        { "fee", floored_subtract(value, spend) }
    };

    if (const auto block = query.find_strong(link); !block.is_terminal())
    {
        size_t position{};
        database::context context{};
        if (!query.get_context(context, block) ||
            !query.get_tx_position(position, link, block))
        {
            send_internal_server_error(database::error::integrity);
            return true;
        }

        object["confirmed"] = boost::json::object
        {
            { "height", context.height },
            { "position", position }
        };
    }

    send_json(std::move(object), 128);
    return true;
}

} // namespace server
} // namespace libbitcoin
