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

#include <atomic>
#include <optional>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

#define CLASS protocol_native
#define SUBSCRIBE_NATIVE(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

// Start.
// ----------------------------------------------------------------------------

void protocol_native::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Chaser subscription is asynchronous, events may be missed.
    subscribe_chase(BIND(handle_chase, _1, _2, _3));

    // Configuration methods.
    SUBSCRIBE_NATIVE(handle_get_configuration, _1, _2, _3, _4);

    // Top methods.
    SUBSCRIBE_NATIVE(handle_get_top, _1, _2, _3, _4);
    SUBSCRIBE_NATIVE(handle_get_top_subscribe, _1, _2, _3, _4, _5);

    // Block methods.
    SUBSCRIBE_NATIVE(handle_get_block, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_header, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_header_context, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_details, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_txs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_block_filter, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_filter_hash, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_filter_header, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_tx, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_NATIVE(handle_get_block_subscribe, _1, _2, _3, _4, _5);

    // Transaction methods.
    SUBSCRIBE_NATIVE(handle_get_tx, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_tx_header, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_tx_details, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_tx_subscribe, _1, _2, _3, _4, _5);

    // Input methods.
    SUBSCRIBE_NATIVE(handle_get_inputs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_input, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_input_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_input_witness, _1, _2, _3, _4, _5, _6);

    // Output methods.
    SUBSCRIBE_NATIVE(handle_get_outputs, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_output, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_spender, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_spenders, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_subscribe, _1, _2, _3, _4, _5, _6, _7);

    // Address methods.
    SUBSCRIBE_NATIVE(handle_get_address, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_confirmed, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_unconfirmed, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_balance, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_subscribe, _1, _2, _3, _4, _5, _6, _7);

    // Admin endpoint methods (TODO: move to admin interface).
    SUBSCRIBE_NATIVE(handle_get_log_subscribe, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_event_subscribe, _1, _2, _3, _4, _5);
    protocol_html::start();
}

void protocol_native::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    dispatcher_.stop(ec);
    unsubscribe_chase();
    protocol_html::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_native::try_dispatch_object(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto target = request.target();
    BC_POP_WARNING()

    rpc::request_t model{};
    if (const auto ec = native_target(model, target))
        return !ec;

    if (!native_query(model, request))
    {
        send_not_acceptable(request);
        return true;
    }

    if (get_media(model) == media_type::text_html)
        return false;

    if (const auto ec = dispatcher_.notify(model))
        send_internal_server_error(ec, request);

    return true;
}

void protocol_native::dispatch_websocket(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Native websocket interface supports only string requests.
    if (!request.body().contains<http::string_value>())
    {
        stop(network::error::not_acceptable);
        return;
    }

    // Target with query string is passed via websocket string body.
    const auto target = request.body().get<http::string_value>();

    rpc::request_t model{};
    if (const auto ec = native_target(model, target))
        return;

    // Default to json by simulating a json accept header (format overrides).
    if (!native_query(model, target, { media_type::application_json }) ||
        get_media(model) == media_type::text_html)
    {
        stop(network::error::not_acceptable);
        return;
    }

    if (const auto ec = dispatcher_.notify(model))
    {
        stop(network::error::internal_server_error);
        return;
    }
}

// Event handlers.
// ----------------------------------------------------------------------------

bool protocol_native::handle_chase(const code&, node::chase event_,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    if (!websocket())
        return true;

    switch (event_)
    {
        case node::chase::organized:
        {
            auto media = top_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                // Increments height above a fork point (start/reorg).
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST(do_top, std::get<node::header_t>(value), media);
            }

            media = block_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                // No block emission for a fork point (start/reorg).
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST(do_block, std::get<node::header_t>(value), media);
            }

            break;
        }
        case node::chase::reorganized:
        {
            const auto media = top_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                // Resets subscriber height to the fork point.
                BC_ASSERT(std::holds_alternative<node::height_t>(value));
                POST(do_top, std::get<node::height_t>(value), media);
            }

            break;
        }
        case node::chase::transaction:
        {
            const auto media = tx_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                BC_ASSERT(std::holds_alternative<node::transaction_t>(value));
                POST(do_transaction, std::get<node::transaction_t>(value), media);
            }

            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

bool protocol_native::handle_log(const code& ec, uint8_t , time_t ,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec) || !log_subscribe_.load(relaxed))
        return false;

    if (!websocket())
        return true;

    // TODO: map subscribed flags to event filter, emit single json event.
    return false;
}

bool protocol_native::handle_events(const code& ec, uint8_t ,
    uint64_t , const logger::time& ) NOEXCEPT
{
    if (stopped(ec) || !event_subscribe_.load(relaxed))
        return false;

    if (!websocket())
        return true;

    // TODO: map subscribed flags to event filter, emit single json event.
    return false;
}

// Utilities.
// ----------------------------------------------------------------------------
// private

using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_native::inject(value& out, std::optional<uint32_t> height,
    const database::header_link& link) const NOEXCEPT
{
    out.as_object().emplace("height", height.has_value() ? height.value() :
        archive().get_height(link).value);
}

BC_POP_WARNING()

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
#if defined(UNDEFINED)
using inpoints = database::inpoints;
using outpoints = database::outpoints;

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

} // namespace server
} // namespace libbitcoin
