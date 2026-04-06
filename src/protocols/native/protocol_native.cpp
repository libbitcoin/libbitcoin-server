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

#include <optional>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace std::placeholders;

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

    // Configuration methods.
    SUBSCRIBE_NATIVE(handle_get_configuration, _1, _2, _3, _4);

    // Block methods.
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

    // Transaction methods.
    SUBSCRIBE_NATIVE(handle_get_tx, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_tx_header, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_tx_details, _1, _2, _3, _4, _5);

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

    // Address methods.
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

    if (get_media(model) == media_type::text_html)
        return false;

    if (const auto ec = dispatcher_.notify(model))
        send_internal_server_error(ec, request);

    return true;
}

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

// Utilities.
// ----------------------------------------------------------------------------
// private

using namespace boost::json;

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
