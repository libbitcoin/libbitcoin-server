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
#include <bitcoin/server/protocols/protocol_bitcoind.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Terminal dispatch.
// ----------------------------------------------------------------------------
// This class is attached to the channel after the subgroup protocols, which
// claim requests defined by their interfaces. Defaults are sent here exactly
// once and only when no subgroup has claimed the request.

// Claimed by rest (when attached), otherwise disallowed.
void protocol_bitcoind::handle_receive_get(const code& ec,
    const http::method::get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (claimed())
        return;

    send_method_not_allowed(*get);
}

// Handled here for rpc and derived rest protocol.
void protocol_bitcoind::handle_receive_options(const code& ec,
    const options::cptr& options) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*options, options->version()))
    {
        send_bad_host(*options);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*options, options->version()))
    {
        send_forbidden(*options);
        return;
    }

    send_ok(*options);
}

// The subgroup protocols handle claimed posts.
void protocol_bitcoind::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // A subgroup protocol has claimed (and responds to) the request.
    if (claimed())
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*post, post->version()))
    {
        send_bad_host(*post);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*post, post->version()))
    {
        send_forbidden(*post);
        return;
    }

    // Endpoint accepts only json-rpc posts.
    if (!post->body().contains<request>())
    {
        send_bad_request(*post);
        return;
    }

    // Get the parsed json-rpc request object (v1 or v2, singleton or batch).
    // v2 missing id is a notification (no response object is sent).
    // v1 null id is also a notification, but bitcoind answers (non-compliant).
    const auto& message = post->body().get<request>().message;

    // Cache request context for response building (version + id).
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_forbidden(*post);
        return;
    }

    // No subgroup interface defines the method.
    send_error(error::bitcoind::method_not_found);
}

// The websocket transport of the interface: subgroup protocols handle claimed
// frames, invalid frames stop the channel here.
void protocol_bitcoind::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // A subgroup protocol has claimed (and responds to) the request.
    if (claimed())
        return;

    // ws frames carry no headers, so ws authorization is enforced here, not
    // by the channel (established by basic auth on the upgrade request).
    if (!authorized())
    {
        stop(network::error::unauthorized);
        return;
    }

    // Websocket frames are parsed as json-rpc requests (channel body type).
    if (!request.body().contains<rpc::request>())
    {
        stop(error::invalid_argument);
        return;
    }

    const auto& message = request.body().get<rpc::request>().message;

    // Cache request context for response building (version + id).
    set_rpc_request(message);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        stop(network::error::unauthorized);
        return;
    }

    // No subgroup interface defines the method.
    send_error(error::bitcoind::method_not_found);
}

// Help.
// ----------------------------------------------------------------------------

// Protocols register their served names with the channel upon start.
std::string protocol_bitcoind::help_names() const NOEXCEPT
{
    return methods();
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_bitcoind::send_error(const code& ec) NOEXCEPT
{
    send_error(ec, two * ec.message().size());
}

void protocol_bitcoind::send_error(const code& ec,
    size_t size_hint) NOEXCEPT
{
    send_error(ec, {}, size_hint);
}

void protocol_bitcoind::send_error(const code& ec, size_t size_hint,
    const code& close_reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .error = result_t
        {
            .code = ec.value(),
            .message = ec.message()
        }
    }, size_hint, close_reason);
}

void protocol_bitcoind::send_error(const code& ec,
    value_option&& error, size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .error = result_t
        {
            .code = ec.value(),
            .message = ec.message(),
            .data = std::move(error)
        }
    }, size_hint);
}

void protocol_bitcoind::send_text(std::string&& hexidecimal) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_result(hexidecimal, hexidecimal.size());
}

void protocol_bitcoind::send_result(value_option&& result,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .result = std::move(result)
    }, size_hint);
}

// private
void protocol_bitcoind::send_rpc(response_t&& model,
    size_t size_hint) NOEXCEPT
{
    send_rpc(std::move(model), size_hint, error::success);
}

void protocol_bitcoind::send_rpc(response_t&& model, size_t size_hint,
    const code& close_reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace http;
    static const auto json = from_media_type(media_type::application_json);

    // A v2 request without an id is a notification (no response object).
    const auto notification = (model.jsonrpc == version::v2) &&
        !model.id.has_value();

    if (websocket())
    {
        id_.reset();
        version_ = version::undefined;

        // An unsent response does not restart the read cycle, so resume it.
        if (notification)
        {
            if (close_reason)
                stop(close_reason);
            else
                network::protocol::resume();

            return;
        }

        http::response message{ status::ok, 11 };
        message.set(field::content_type, json);
        message.body() = rpc::response
        {
            { .size_hint = size_hint }, std::move(model),
        };
        message.prepare_payload();
        SEND(std::move(message), handle_complete, _1, close_reason);
        return;
    }

    const auto request = reset_rpc_request();
    const auto& body = request->body().get<rpc::request>();

    // A batched notification is answered (response parts are sequenced).
    if (notification && !body.batch && !body.changed)
    {
        http::response message{ status::no_content, request->version() };
        add_common_headers(message, *request);
        add_access_control_headers(message, *request);
        SEND(std::move(message), handle_complete, _1, close_reason);
        return;
    }

    http::response message{ status::ok, request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(field::content_type, json);
    message.body() = rpc::response
    {
        { .size_hint = size_hint }, std::move(model),
    };
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, close_reason);
}

// protected
void protocol_bitcoind::set_rpc_request(version version,
    const id_option& id, const http::request_cptr& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    id_ = id;
    version_ = version;
    set_request(request);
}

// protected
void protocol_bitcoind::set_rpc_request(const request_t& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    id_ = message.id;
    version_ = message.jsonrpc;
}

// private
http::request_cptr protocol_bitcoind::reset_rpc_request() NOEXCEPT
{
    BC_ASSERT(stranded());
    id_.reset();
    version_ = version::undefined;
    return reset_request();
}

// Utility (redundant with protocol_electrum).
// ----------------------------------------------------------------------------

code protocol_bitcoind::validate_tx(
    const chain::transaction& tx) const NOEXCEPT
{
    const auto& query = archive();
    const auto& settings = system_settings();
    const auto link = query.to_confirmed(query.get_top_confirmed());
    const auto key = query.get_header_key(link);
    const auto state = query.get_chain_state(settings, key);

    // The store always has chain state for the confirmed top.
    if (!state)
        return database::error::integrity;

    // The context of the next block, in which a pool tx would confirm.
    const auto pool = chain::chain_state{ *state, settings }.context();
    return node::validate_transaction(tx, query, pool);
}

code protocol_bitcoind::broadcast_tx(
    const chain::transaction::cptr& tx) NOEXCEPT
{
    if (const auto ec = validate_tx(*tx))
        return ec;

    BROADCAST(peer::transaction, to_shared<peer::transaction>(tx));
    return error::success;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
