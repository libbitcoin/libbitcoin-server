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
#include <ranges>
#include <utility>
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

// TODO: rationalize confirmed state against database (to_block vs. to_strong).

namespace libbitcoin {
namespace server {

#define CLASS protocol_native
#define SUBSCRIBE_NATIVE(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)
    
using namespace system;
using namespace network;
using namespace network::messages::peer;
using namespace std::placeholders;
using namespace boost::json;

using inpoint = database::inpoint;
using outpoint = database::outpoint;
using inpoints = database::inpoints;
using outpoints = database::outpoints;

// Avoiding namespace conflict.
using object_type = network::rpc::object_t;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_native::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_NATIVE(handle_get_configuration, _1, _2, _3, _4);

    SUBSCRIBE_NATIVE(handle_get_top, _1, _2, _3, _4);
    SUBSCRIBE_NATIVE(handle_get_block, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_header, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_header_context, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_details, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_txs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_filter, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_filter_hash, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_filter_header, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_tx, _1, _2, _3, _4, _5, _6, _7, _8);

    SUBSCRIBE_NATIVE(handle_get_tx, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_tx_header, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_tx_details, _1, _2, _3, _4, _5);

    SUBSCRIBE_NATIVE(handle_get_inputs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_input, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_input_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_input_witness, _1, _2, _3, _4, _5, _6);

    SUBSCRIBE_NATIVE(handle_get_outputs, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_output, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_spender, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_spenders, _1, _2, _3, _4, _5, _6);

    SUBSCRIBE_NATIVE(handle_get_address, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_confirmed, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_unconfirmed, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_balance, _1, _2, _3, _4, _5, _6);
    protocol_html::start();
}

void protocol_native::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    dispatcher_.stop(ec);
    protocol_html::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_native::try_dispatch_object(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    rpc::request_t model{};
    if (const auto ec = native_target(model, request.target()))
    {
        LOGA("Request parse [" << request.target() << "] " << ec.message());
        return !ec;
    }

    if (!native_query(model, request))
    {
        send_not_acceptable(request);
        return true;
    }

    if (get_media(model) == http::media_type::text_html)
        return false;

    if (const auto ec = dispatcher_.notify(model))
        send_internal_server_error(ec, request);

    return true;
}

// Serialization.
// ----------------------------------------------------------------------------

////constexpr auto html = to_value(http::media_type::text_html);
constexpr auto text = to_value(http::media_type::text_plain);
constexpr auto json = to_value(http::media_type::application_json);
constexpr auto data = to_value(http::media_type::application_octet_stream);

// Handlers.
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_configuration(const code& ec,
    interface::configuration, uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    boost::json::object object
    {
        { "address", archive().address_enabled() },
        { "filter", archive().filter_enabled() },
        { "turbo", database_settings().turbo },
        { "witness", network_settings().witness_node() },
        { "retarget", system_settings().forks.retarget },
        { "difficult", system_settings().forks.difficult },
    };

    send_json(std::move(object), 32);
    return true;
}

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

    const auto link = to_header(height, hash);
    if (const auto block = archive().get_block(link, witness))
    {
        const auto size = block->serialized_size(witness);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*block, size, witness));
                return true;
            case text:
                send_text(to_hex(*block, size, witness));
                return true;
            case json:
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
        { "claim", claim },
        { "fees", fees },
        { "subsidy", subsidy },
        { "reward", ceilinged_add(fees, subsidy) }
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
                const auto data = pointer_cast<const uint8_t>(hashes.data());
                send_chunk(to_chunk({ data, std::next(data, size) }));
                return true;
            }
            case text:
            {
                const auto data = pointer_cast<const uint8_t>(hashes.data());
                send_text(encode_base16({ data, std::next(data, size) }));
                return true;
            }
            case json:
            {
                array out(hashes.size());
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
    const auto link = query.to_confirmed_block(*hash);
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
    if (!query.get_tx_sizes(nominal, maximal, link) ||
        !query.get_tx_value(value, link) ||
        !query.get_tx_spend(spend, link) ||
        is_subtract_overflow(value, spend))
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    // sigops and wtxid are not cached, so removed for now.
    boost::json::object object
    {
        { "segregated", maximal != nominal },
        { "coinbase", query.is_coinbase(link) },
        { "nominal", nominal },
        { "maximal", maximal },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "virtual", chain::virtual_size(nominal, maximal) },
        { "value", value },
        { "spend", spend },
        { "fee", floored_subtract(value, spend) }
    };

    size_t position{};
    if (query.get_tx_position(position, link))
    {
        database::context context{};
        if (!query.get_context(context, query.to_strong(link)))
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
    const auto spender = query.get_spender(query.to_confirmed_spender(spent));
    if (spender.index() == chain::point::null_index)
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

    const auto size = ins.size() * inpoint::serialized_size();
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

// handle_get_address
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address(const code& ec, interface::address,
    uint8_t, uint8_t media, const hash_cptr& hash, bool turbo) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_address, media, turbo, hash);
    return true;
}

// private
void protocol_native::do_get_address(uint8_t media, bool turbo,
    const hash_cptr& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    outpoints set{};
    const auto& query = archive();
    const auto ec = query.get_address_outputs(stopping_, set, *hash, turbo);
    POST(complete_get_address, ec, media, std::move(set));
}

// This is shared by the three get_address... methods.
void protocol_native::complete_get_address(const code& ec, uint8_t media,
    const outpoints& set) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Stop monitoring socket.
    monitor(false);

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    if (set.empty())
    {
        send_not_found();
        return;
    }

    const auto size = set.size() * chain::outpoint::serialized_size();
    switch (media)
    {
        case data:
            send_chunk(to_bin_array(set, size));
            return;
        case text:
            send_text(to_hex_array(set, size));
            return;
        case json:
            send_json(value_from(set), two * size);
            return;
    }

    send_not_found();
}

// handle_get_address_confirmed
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address_confirmed(const code& ec,
    interface::address_confirmed, uint8_t, uint8_t media,
    const hash_cptr& hash, bool turbo) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_address_confirmed, media, turbo, hash);
    return true;
}

// private
void protocol_native::do_get_address_confirmed(uint8_t media, bool turbo,
    const hash_cptr& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    outpoints set{};
    const auto& query = archive();
    auto ec = query.get_confirmed_unspent_outputs(stopping_, set, *hash, turbo);
    POST(complete_get_address, ec, media, std::move(set));
}

// handle_get_address_unconfirmed
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address_unconfirmed(const code& ec,
    interface::address_unconfirmed, uint8_t, uint8_t,
    const hash_cptr&, bool) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // TODO: there are currently no unconfirmed txs.
    send_not_implemented();
    return true;
}

// handle_get_address_balance
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address_balance(const code& ec,
    interface::address_balance, uint8_t, uint8_t media,
    const hash_cptr& hash, bool turbo) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.address_enabled())
    {
        send_not_implemented();
        return true;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_address_balance, media, turbo, hash);
    return true;
}

void protocol_native::do_get_address_balance(uint8_t media, bool turbo,
    const hash_cptr& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    uint64_t balance{};
    const auto& query = archive();
    auto ec = query.get_confirmed_balance(stopping_, balance, *hash, turbo);
    POST(complete_get_address_balance, ec, media, balance);
}

void protocol_native::complete_get_address_balance(const code& ec,
    uint8_t media, uint64_t balance) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Stop monitoring socket.
    monitor(false);

    // Suppresses cancelation error response.
    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    switch (media)
    {
        case data:
            send_chunk(to_little_endian_size(balance));
            return;
        case text:
            send_text(encode_base16(to_little_endian_size(balance)));
            return;
        case json:
            send_json(balance, two * sizeof(balance));
            return;
    }

    send_not_found();
}

// Utilities.
// ----------------------------------------------------------------------------
// private

void protocol_native::inject(value& out, std::optional<uint32_t> height,
    const database::header_link& link) const NOEXCEPT
{
    out.as_object().emplace("height", height.has_value() ? height.value() :
        archive().get_height(link).value);
}

database::header_link protocol_native::to_header(
    const std::optional<uint32_t>& height,
    const std::optional<hash_cptr>& hash) NOEXCEPT
{
    const auto& query = archive();

    if (hash.has_value())
        return query.to_header(*(hash.value()));

    if (height.has_value())
        return query.to_confirmed(height.value());

    return {};
}

// Use if deserialization is required.
#if defined (UNDEFINED)
DECLARE_JSON_TAG_INVOKE(inpoints);
DECLARE_JSON_TAG_INVOKE(outpoints);

DEFINE_JSON_TO_TAG(inpoints)
{
    inpoints out{};
    for (const auto& point: value.as_array())
        out.insert(value_to<inpoint>(point));

    return out;
}

DEFINE_JSON_TO_TAG(outpoints)
{
    outpoints out{};
    for (const auto& point: value.as_array())
        out.insert(value_to<outpoint>(point));

    return out;
}
#endif 

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
