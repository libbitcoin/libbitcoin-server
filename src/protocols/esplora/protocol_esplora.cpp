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
#include <bitcoin/server/protocols/protocol_esplora.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::http;
using namespace std::placeholders;

#define CLASS protocol_esplora
#define SUBSCRIBE_ESPLORA(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

// Shared pointers required in handler parameters so closures control lifetime.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Start.
// ----------------------------------------------------------------------------

void protocol_esplora::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Block methods.
    SUBSCRIBE_ESPLORA(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_ESPLORA(handle_get_block_header, _1, _2, _3, _4);
    SUBSCRIBE_ESPLORA(handle_get_block_status, _1, _2, _3, _4);
    SUBSCRIBE_ESPLORA(handle_get_block_txids, _1, _2, _3, _4);
    SUBSCRIBE_ESPLORA(handle_get_block_txid, _1, _2, _3, _4, _5);
    SUBSCRIBE_ESPLORA(handle_get_block_height, _1, _2, _3, _4);
    SUBSCRIBE_ESPLORA(handle_get_blocks, _1, _2, _3, _4);
    SUBSCRIBE_ESPLORA(handle_get_tip_height, _1, _2, _3);
    SUBSCRIBE_ESPLORA(handle_get_tip_hash, _1, _2, _3);

    // Mempool methods.
    SUBSCRIBE_ESPLORA(handle_get_mempool, _1, _2, _3);
    SUBSCRIBE_ESPLORA(handle_get_mempool_txids, _1, _2, _3);
    SUBSCRIBE_ESPLORA(handle_get_mempool_recent, _1, _2, _3);
    SUBSCRIBE_ESPLORA(handle_get_fee_estimates, _1, _2, _3);
    protocol_http::start();
}

void protocol_esplora::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    dispatcher_.stop(ec);
    protocol_http::stopping(ec);
}

// A parsed target with no subscriber would leave the request unanswered.
bool protocol_esplora::is_implemented(const std::string& method) NOEXCEPT
{
    return
        method == interface::block::name ||
        method == interface::block_header::name ||
        method == interface::block_status::name ||
        method == interface::block_txids::name ||
        method == interface::block_txid::name ||
        method == interface::block_height::name ||
        method == interface::blocks::name ||
        method == interface::tip_height::name ||
        method == interface::tip_hash::name ||
        method == interface::mempool::name ||
        method == interface::mempool_txids::name ||
        method == interface::mempool_recent::name ||
        method == interface::fee_estimates::name;
}

// Handle get method.
// ----------------------------------------------------------------------------

void protocol_esplora::handle_receive_get(const code& ec,
    const method::get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!is_origin_form(get->target()))
    {
        send_bad_target({}, *get);
        return;
    }

    if (!is_allowed_origin(*get, get->version()))
    {
        send_forbidden(*get);
        return;
    }

    if (!is_allowed_host(*get, get->version()))
    {
        send_bad_host(*get);
        return;
    }

    if (!try_dispatch_object(*get))
        send_not_found(*get);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_esplora::try_dispatch_object(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto target = request.target();
    BC_POP_WARNING()

    rpc::request_t model{};
    if (esplora_target(model, target))
        return false;

    if (!is_implemented(model.method))
    {
        send_not_implemented(request);
        return true;
    }

    if (const auto ec = dispatcher_.notify(model))
        send_internal_server_error(ec, request);

    return true;
}

void protocol_esplora::dispatch_websocket(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!authorized())
    {
        stop(network::error::unauthorized);
        return;
    }

    if (!request.body().contains<http::string_value>())
    {
        stop(network::error::not_acceptable);
        return;
    }

    const auto target = request.body().get<http::string_value>();

    rpc::request_t model{};
    if (esplora_target(model, target))
    {
        stop(network::error::bad_request);
        return;
    }

    if (!is_implemented(model.method))
    {
        stop(network::error::not_implemented);
        return;
    }

    if (dispatcher_.notify(model))
        stop(network::error::internal_server_error);
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_esplora::send_json(boost::json::value&& model, size_t size_hint,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    const auto json = from_media_type(media_type::application_json);
    response.set(field::content_type, json);
    response.body() = json_value
    {
        .model = std::move(model),
        .size_hint = size_hint
    };
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_esplora::send_text(std::string&& text,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    const auto plain = from_media_type(media_type::text_plain);
    response.set(field::content_type, plain);
    response.body() = std::move(text);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_esplora::send_chunk(system::data_chunk&& bytes,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    const auto octets = from_media_type(media_type::application_octet_stream);
    response.set(field::content_type, octets);
    response.body() = std::move(bytes);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
