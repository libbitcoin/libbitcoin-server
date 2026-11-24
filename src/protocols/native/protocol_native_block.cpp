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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <atomic>
#include <optional>
#include <ranges>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::messages::peer;

BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_native::handle_get_top(const code& ec, interface::top,
    uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return;

    switch (media)
    {
        case data:
            send_chunk(to_tops());
            return;
        case text:
            send_text(encode_base16(to_tops()));
            return;
        case json:
            send_json(to_tops_json());
            return;
    }

    send_not_found();
}

void protocol_native::handle_get_top_subscribe(const code& ec,
    interface::top_subscribe, uint8_t version, uint8_t media,
    bool stop) NOEXCEPT
{
    if (stopped(ec))
        return;

    top_subscribe_.store(to_media(stop, media), std::memory_order_relaxed);
    if (stop)
    {
        send_empty();
        return;
    }

    return handle_get_top(ec, {}, version, media);
}

void protocol_native::handle_get_top_chain(const code& ec,
    interface::top_chain, uint8_t, uint8_t media, bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    switch (media)
    {
        case data:
            send_chunk(to_top(candidate));
            return;
        case text:
            send_text(encode_base16(to_top(candidate)));
            return;
        case json:
            send_json(to_top_json(candidate));
            return;
    }

    send_not_found();
}

void protocol_native::handle_get_top_height(const code& ec,
    interface::top_height, uint8_t, uint8_t media, bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto height = possible_narrow_cast<uint32_t>(candidate ?
        query.get_top_candidate() : query.get_top_confirmed());

    switch (media)
    {
        case data:
            send_chunk(to_chunk(to_little_endian(height)));
            return;
        case text:
            send_text(encode_base16(to_little_endian(height)));
            return;
        case json:
            send_json(height);
            return;
    }

    send_not_found();
}

// notify
void protocol_native::do_top(media_type media) NOEXCEPT
{
    BC_ASSERT(stranded());

    switch (to_value(media))
    {
        case data:
            notify_chunk(to_tops());
            return;
        case text:
            notify_text(encode_base16(to_tops()));
            return;
        case json:
            notify_json(to_tops_json());
            return;
    }
}

void protocol_native::handle_get_block(const code& ec, interface::block,
    uint8_t, uint8_t media, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool witness, bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = to_header(height, hash, candidate);
    if (link.is_terminal() || query.is_pruned(link))
    {
        send_not_found();
        return;
    }

    size_t size{};
    if (!query.get_block_size(size, link, witness))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    switch (media)
    {
        case data:
        {
            send_stream(size, [&query, link, witness, turbo = turbo_](
                const data_slab& out) NOEXCEPT
            {
                return query.get_wire_block(out, link, witness, turbo);
            }, media_type::application_octet_stream);
            return;
        }
        case text:
        {
            send_stream(two * size, [&query, link, witness, turbo = turbo_](
                const data_slab& out) NOEXCEPT
            {
                return query.get_wire_block<write::base16::fast>(out, link,
                    witness, turbo);
            }, media_type::text_plain);
            return;
        }
        case json:
        {
            const auto block = query.get_block(link, witness);
            if (!block)
            {
                send_internal_server_error(database::error::integrity);
                return;
            }

            auto model = value_from(block);
            inject(model.at("header"), height, link);
            send_json(std::move(model));
            return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_header(const code& ec,
    interface::block_header, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto link = to_header(height, hash, candidate);
    if (const auto header = archive().get_header(link))
    {
        constexpr auto size = chain::header::serialized_size();
        switch (media)
        {
            case data:
                send_chunk(to_bin(*header, size));
                return;
            case text:
                send_text(to_hex(*header, size));
                return;
            case json:
                auto model = value_from(header);
                inject(model, height, link);
                send_json(std::move(model));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_header_context(const code& ec,
    interface::block_header_context, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (media != json)
    {
        send_not_acceptable();
        return;
    }

    database::context context{};
    const auto& query = archive();
    const auto link = to_header(height, hash, candidate);
    if (!query.get_context(context, link))
    {
        send_not_found();
        return;
    }

    system::json::object object
    {
        { "hash", encode_hash(query.get_header_key(link)) },
        { "height", context.height },
        { "mtp", context.mtp },
    };

    // The "state" element implies transactions are associated.
    if (query.is_associated(link))
    {
        size_t nominal{}, maximal{};
        if (!query.get_block_sizes(nominal, maximal, link))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        const auto bypass = query.is_bypass(link);

        object["state"] = system::json::object
        {
            { "size", nominal },
            { "weight", chain::weighted_size(nominal, maximal) },
            { "count", query.get_tx_count(link) },
            { "validated", bypass || query.is_validated(link) },
            { "confirmed", query.is_confirmed_block(link) },
            { "confirmable", bypass || query.is_confirmable(link) },
            { "unconfirmable", !bypass && query.is_unconfirmable(link) }
        };
    }

    // TODO: move to system serializer.
    // All modern configurable forks.
    object["forks"] = system::json::object
    {
        { "bip16",  context.is_enabled(chain::flags::bip16_rule) },
        { "bip30",  context.is_enabled(chain::flags::bip30_rule) },
        { "bip34",  context.is_enabled(chain::flags::bip34_rule) },
        { "bip66",  context.is_enabled(chain::flags::bip66_rule) },
        { "bip65",  context.is_enabled(chain::flags::bip65_rule) },
        { "bip68",  context.is_enabled(chain::flags::bip68_rule) },
        { "bip112", context.is_enabled(chain::flags::bip112_rule) },
        { "bip113", context.is_enabled(chain::flags::bip113_rule) },
        { "bip141", context.is_enabled(chain::flags::bip141_rule) },
        { "bip143", context.is_enabled(chain::flags::bip143_rule) },
        { "bip147", context.is_enabled(chain::flags::bip147_rule) },
        { "bip42",  context.is_enabled(chain::flags::bip42_rule) },
        { "bip341", context.is_enabled(chain::flags::bip341_rule) },
        { "bip342", context.is_enabled(chain::flags::bip342_rule) },
        { "bip54_timewarp", context.is_enabled(chain::flags::bip54_timewarp_rule) },
        { "bip54_timespan", context.is_enabled(chain::flags::bip54_timespan_rule) },
        { "bip54_sigops", context.is_enabled(chain::flags::bip54_sigops_rule) },
        { "bip54_txsize", context.is_enabled(chain::flags::bip54_txsize_rule) },
        { "bip54_coinbase", context.is_enabled(chain::flags::bip54_coinbase_rule) }
    };

    send_json(std::move(object));
}

void protocol_native::handle_get_block_details(const code& ec,
    interface::block_details, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (media != json)
    {
        send_not_acceptable();
        return;
    }

    const auto& query = archive();
    const auto link = to_header(height, hash, candidate);

    // Missing block.
    if (!query.is_associated(link))
    {
        send_not_found();
        return;
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
        return;
    }

    const auto fees = floored_subtract(value, spend);
    const auto bip42 = context.is_enabled(chain::flags::bip42_rule);
    const auto subsidy = to_subsidy(context.height, bip42);

    // sigops is not cached, so removed for now.
    system::json::object object
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

    send_json(std::move(object));
}

void protocol_native::handle_get_block_txs(const code& ec,
    interface::block_txs, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = to_header(height, hash, candidate);
    if (const auto hashes = query.get_tx_keys(link); !hashes.empty())
    {
        const auto size = hashes.size() * hash_size;
        switch (media)
        {
            case data:
            {
                const auto bytes = pointer_cast<const uint8_t>(hashes.data());
                send_chunk(to_chunk({ bytes, std::next(bytes, size) }));
                return;
            }
            case text:
            {
                const auto bytes = pointer_cast<const uint8_t>(hashes.data());
                send_text(encode_base16({ bytes, std::next(bytes, size) }));
                return;
            }
            case json:
            {
                system::json::array out(hashes.size());
                std::ranges::transform(hashes, out.begin(),
                    [](const auto& hash) { return encode_hash(hash); });
                send_json(out);
                return;
            }
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_txs_details(const code& ec,
    interface::block_txs_details, uint8_t, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (media != json)
    {
        send_not_acceptable();
        return;
    }

    const auto& query = archive();
    const auto link = to_header(height, hash, candidate);

    // Missing block.
    if (!query.is_associated(link))
    {
        send_not_found();
        return;
    }

    system::json::array out{};
    database::context context{};
    const auto txs = query.to_transactions(link);
    if (txs.empty() || !to_tx_details(out, txs, to_roots(txs)) ||
        !query.get_context(context, link))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    uint64_t fees{};
    for (const auto& tx: out)
        fees = ceilinged_add(fees, tx.as_object().at("fee").to_number<uint64_t>());

    const auto bip42 = context.is_enabled(chain::flags::bip42_rule);
    out.front().as_object()["value"] = ceilinged_add(
        to_subsidy(context.height, bip42), fees);

    send_json(std::move(out));
}

void protocol_native::handle_get_block_filter(const code& ec,
    interface::block_filter, uint8_t, uint8_t media, uint8_t type,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    if (!query.filter_enabled() || type != client_filter::type_id::neutrino)
    {
        send_not_implemented();
        return;
    }

    data_chunk filter{};
    if (query.get_filter_body(filter, to_header(height, hash, candidate)))
    {
        switch (media)
        {
            case data:
                send_chunk(std::move(filter));
                return;
            case text:
                send_text(encode_base16(filter));
                return;
            case json:
                send_json(value_from(encode_base16(filter)));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_filter_hash(const code& ec,
    interface::block_filter_hash, uint8_t, uint8_t media, uint8_t type,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    if (!query.filter_enabled() || type != client_filter::type_id::neutrino)
    {
        send_not_implemented();
        return;
    }

    hash_digest filter_hash{ hash_size };
    if (query.get_filter_hash(filter_hash, to_header(height, hash, candidate)))
    {
        switch (media)
        {
            case data:
                send_chunk(to_chunk(filter_hash));
                return;
            case text:
                send_text(encode_base16(filter_hash));
                return;
            case json:
                send_json(value_from(encode_hash(filter_hash)));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_filter_header(const code& ec,
    interface::block_filter_header, uint8_t, uint8_t media, uint8_t type,
    std::optional<hash_cptr> hash, std::optional<uint32_t> height,
    bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    if (!query.filter_enabled() || type != client_filter::type_id::neutrino)
    {
        send_not_implemented();
        return;
    }

    hash_digest filter_head{ hash_size };
    if (query.get_filter_head(filter_head, to_header(height, hash, candidate)))
    {
        switch (media)
        {
            case data:
                send_chunk(to_chunk(filter_head));
                return;
            case text:
                send_text(encode_base16(filter_head));
                return;
            case json:
                send_json(value_from(encode_hash(filter_head)));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_tx(const code& ec, interface::block_tx,
    uint8_t, uint8_t media, uint32_t position, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool witness, bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = query.to_transaction(to_header(height, hash, candidate),
        position);
    if (query.is_pruned(link))
    {
        send_not_found();
        return;
    }

    if (const auto tx = query.get_transaction(link, witness))
    {
        const auto size = tx->serialized_size(witness);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*tx, size, witness));
                return;
            case text:
                send_text(to_hex(*tx, size, witness));
                return;
            case json:
                send_json(value_from(tx));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_tx_input_script(const code& ec,
    interface::block_tx_input_script, uint8_t, uint8_t media,
    uint32_t position, uint32_t index, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = query.to_transaction(to_header(height, hash, candidate),
        position);
    if (query.is_pruned(link))
    {
        send_not_found();
        return;
    }

    if (const auto script = query.get_input_script(query.to_point(link, index)))
    {
        const auto size = script->serialized_size(false);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*script, size, false));
                return;
            case text:
                send_text(to_hex(*script, size, false));
                return;
            case json:
                send_json(value_from(script));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_tx_output_script(const code& ec,
    interface::block_tx_output_script, uint8_t, uint8_t media,
    uint32_t position, uint32_t index, std::optional<hash_cptr> hash,
    std::optional<uint32_t> height, bool candidate) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto tx = query.to_transaction(to_header(height, hash, candidate),
        position);

    if (const auto script = query.get_output_script(query.to_output(tx, index)))
    {
        const auto size = script->serialized_size(false);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*script, size, false));
                return;
            case text:
                send_text(to_hex(*script, size, false));
                return;
            case json:
                send_json(value_from(script));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_block_subscribe(const code& ec,
    interface::block_subscribe, uint8_t, uint8_t media, bool stop) NOEXCEPT
{
    if (stopped(ec))
        return;

    block_subscribe_.store(to_media(stop, media), std::memory_order_relaxed);
    if (stop)
    {
        send_empty();
        return;
    }

    // Return top block hash upon block subscription.
    const auto& query = archive();
    const auto link = query.to_top_confirmed();
    if (const auto hash = query.get_header_key(link); hash != null_hash)
    {
        switch (media)
        {
            case data:
                send_chunk(to_chunk(hash));
                return;
            case text:
                send_text(encode_base16(hash));
                return;
            case json:
                send_json(value_from(encode_hash(hash)));
                return;
        }
    }

    send_not_found();
}

// notify
void protocol_native::do_block(node::header_t link, media_type media) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto hash = archive().get_header_key(link);
    switch (to_value(media))
    {
        case data:
            notify_chunk(to_chunk(hash));
            return;
        case text:
            notify_text(encode_base16(hash));
            return;
        case json:
            notify_json(value_from(encode_hash(hash)));
            return;
    }
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
