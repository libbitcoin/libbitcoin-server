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

#include <iterator>
#include <optional>
#include <ranges>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::messages::peer;

BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)

bool protocol_native::handle_get_top(const code& ec, interface::top,
    uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto height = archive().get_top_confirmed();
    switch (media)
    {
        case data:
            send_chunk(to_little_endian_size(height));
            return true;
        case text:
            send_text(encode_base16(to_little_endian_size(height)));
            return true;
        case json:
            send_json(height, two * sizeof(height));
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block(const code& ec, interface::block,
    uint8_t, uint8_t media, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = to_header(height, hash);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    size_t size{};
    if (!query.get_block_size(size, link, witness))
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    switch (media)
    {
        case data:
        {
            data_chunk out(size);
            stream::out::fast sink{ out };
            write::bytes::fast writer{ sink };
            if (!query.get_wire_block(writer, link, witness))
            {
                send_internal_server_error(database::error::integrity);
                return true;
            }

            send_chunk(std::move(out));
            return true;
        }
        case text:
        {
            std::string out(two * size, '\0');
            stream::out::fast sink{ out };
            write::base16::fast writer{ sink };
            if (!query.get_wire_block(writer, link, witness))
            {
                send_internal_server_error(database::error::integrity);
                return true;
            }

            send_text(std::move(out));
            return true;
        }
        case json:
        {
            const auto block = query.get_block(link, witness);
            if (!block)
            {
                send_internal_server_error(database::error::integrity);
                return true;
            }

            auto model = value_from(block);
            inject(model.at("header"), height, link);
            send_json(std::move(model), two * size);
            return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block_header(const code& ec,
    interface::block_header, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto link = to_header(height, hash);
    if (const auto header = archive().get_header(link))
    {
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
                inject(model, height, link);
                send_json(std::move(model), two * size);
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block_header_context(const code& ec,
    interface::block_header_context, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    database::context context{};
    const auto& query = archive();
    const auto link = to_header(height, hash);
    if (!query.get_context(context, link))
    {
        send_not_found();
        return true;
    }

    boost::json::object object
    {
        { "hash", encode_hash(query.get_header_key(link)) },
        { "height", context.height },
        { "mtp", context.mtp },
    };

    // The "state" element implies transactions are associated.
    if (query.is_associated(link))
    {
        size_t size{}, weight{};
        if (!query.get_block_sizes(size, weight, link))
        {
            send_internal_server_error(database::error::integrity);
            return true;
        }

        const auto check = system_settings().top_checkpoint().height();
        const auto bypass = context.height < check || query.is_milestone(link);

        object["state"] = boost::json::object
        {
            { "size", size },
            { "weight", weight },
            { "count", query.get_tx_count(link) },
            { "validated", bypass || query.is_validated(link) },
            { "confirmed", check || query.is_confirmed_block(link) },
            { "confirmable", bypass || query.is_confirmable(link) },
            { "unconfirmable", !bypass && query.is_unconfirmable(link) }
        };
    }

    // TODO: move to system serializer.
    // All modern configurable forks.
    object["forks"] = boost::json::object
    {
        { "bip30",  context.is_enabled(chain::flags::bip30_rule) },
        { "bip34",  context.is_enabled(chain::flags::bip34_rule) },
        { "bip66",  context.is_enabled(chain::flags::bip66_rule) },
        { "bip65",  context.is_enabled(chain::flags::bip65_rule) },
        { "bip90",  context.is_enabled(chain::flags::bip90_rule) },
        { "bip68",  context.is_enabled(chain::flags::bip68_rule) },
        { "bip112", context.is_enabled(chain::flags::bip112_rule) },
        { "bip113", context.is_enabled(chain::flags::bip113_rule) },
        { "bip141", context.is_enabled(chain::flags::bip141_rule) },
        { "bip143", context.is_enabled(chain::flags::bip143_rule) },
        { "bip147", context.is_enabled(chain::flags::bip147_rule) },
        { "bip42",  context.is_enabled(chain::flags::bip42_rule) },
        { "bip341", context.is_enabled(chain::flags::bip341_rule) },
        { "bip342", context.is_enabled(chain::flags::bip342_rule) }
    };

    send_json(std::move(object), 256);
    return true;
}

bool protocol_native::handle_get_block_details(const code& ec,
    interface::block_details, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = to_header(height, hash);

    // Missing block.
    if (!query.is_associated(link))
    {
        send_not_found();
        return true;
    }

    const auto count = query.get_tx_count(link);
    const auto key = hash.has_value() ? *hash.value() :
        query.get_header_key(link);

    database::context context{};
    size_t nominal{}, maximal{};
    uint64_t value{}, spend{}, claim{};
    if (is_zero(count) || key == system::null_hash ||
        !query.get_tx_spend(claim, query.to_coinbase(link)) ||
        !query.get_block_sizes(nominal, maximal, link) ||
        !query.get_block_value(value, link) ||
        !query.get_block_spend(spend, link) ||
        !query.get_context(context, link) ||
        is_subtract_overflow(value, spend))
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    const auto fees = floored_subtract(value, spend);
    const auto& settings = system_settings();
    const auto bip42 = context.is_enabled(chain::flags::bip42_rule);
    const auto subsidy = chain::block::subsidy(context.height,
        settings.subsidy_interval_blocks, settings.initial_subsidy(), bip42);

    // sigops is not cached, so removed for now.
    boost::json::object object
    {
        { "hash", encode_hash(key) },
        { "height", context.height },
        { "count", count },
        { "segregated", maximal != nominal },
        { "nominal", nominal },
        { "maximal", maximal },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "virtual", chain::virtual_size(nominal, maximal) },
        { "value", value },
        { "fees", fees },
        { "subsidy", subsidy },
        { "reward", ceilinged_add(fees, subsidy) },
        { "claim", claim }
    };

    send_json(std::move(object), 512);
    return true;
}

bool protocol_native::handle_get_block_txs(const code& ec,
    interface::block_txs, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto hashes = query.get_tx_keys(to_header(height, hash));
        !hashes.empty())
    {
        const auto size = hashes.size() * hash_size;
        switch (media)
        {
            case data:
            {
                const auto bytes = pointer_cast<const uint8_t>(hashes.data());
                send_chunk(to_chunk({ bytes, std::next(bytes, size) }));
                return true;
            }
            case text:
            {
                const auto bytes = pointer_cast<const uint8_t>(hashes.data());
                send_text(encode_base16({ bytes, std::next(bytes, size) }));
                return true;
            }
            case json:
            {
                boost::json::array out(hashes.size());
                std::ranges::transform(hashes, out.begin(),
                    [](const auto& hash) { return encode_hash(hash); });
                send_json(out, two * size);
                return true;
            }
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block_filter(const code& ec,
    interface::block_filter, uint8_t, uint8_t media, uint8_t type,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.filter_enabled() || type != client_filter::type_id::neutrino)
    {
        send_not_implemented();
        return true;
    }

    data_chunk filter{};
    if (query.get_filter_body(filter, to_header(height, hash)))
    {
        switch (media)
        {
            case data:
                send_chunk(std::move(filter));
                return true;
            case text:
                send_text(encode_base16(filter));
                return true;
            case json:
                send_json(value_from(encode_base16(filter)),
                    two * filter.size());
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block_filter_hash(const code& ec,
    interface::block_filter_hash, uint8_t, uint8_t media, uint8_t type,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.filter_enabled() || type != client_filter::type_id::neutrino)
    {
        send_not_implemented();
        return true;
    }

    hash_digest filter_hash{ hash_size };
    if (query.get_filter_hash(filter_hash, to_header(height, hash)))
    {
        switch (media)
        {
            case data:
                send_chunk(to_chunk(filter_hash));
                return true;
            case text:
                send_text(encode_base16(filter_hash));
                return true;
            case json:
                send_json(value_from(encode_hash(filter_hash)),
                    two * hash_size);
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block_filter_header(const code& ec,
    interface::block_filter_header, uint8_t, uint8_t media, uint8_t type,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.filter_enabled() || type != client_filter::type_id::neutrino)
    {
        send_not_implemented();
        return true;
    }

    hash_digest filter_head{ hash_size };
    if (query.get_filter_head(filter_head, to_header(height, hash)))
    {
        switch (media)
        {
            case data:
                send_chunk(to_chunk(filter_head));
                return true;
            case text:
                send_text(encode_base16(filter_head));
                return true;
            case json:
                send_json(value_from(encode_hash(filter_head)),
                    two * hash_size);
                return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_native::handle_get_block_tx(const code& ec, interface::block_tx,
    uint8_t, uint8_t media, uint32_t position, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool witness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (const auto tx = query.get_transaction(query.to_transaction(
        to_header(height, hash), position), witness))
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

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
