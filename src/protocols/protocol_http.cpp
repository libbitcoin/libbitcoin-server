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
#include <bitcoin/server/protocols/protocol_http.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_http

using namespace network::http;
using namespace std::placeholders;

// Shared pointers required in handler parameters so closures control lifetime.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)

// Websocket dispatch.
// ----------------------------------------------------------------------------

void protocol_http::handle_receive_websocket(const code& ec,
    const method::websocket::cptr& websocket) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    dispatch_websocket(*websocket);
}

void protocol_http::handle_receive_stream(const code& ec,
    const method::stream::cptr& stream) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    dispatch_websocket(*stream);
}

void protocol_http::dispatch_websocket(const request& ) NOEXCEPT
{
    BC_ASSERT(stranded());
    stop(error::not_implemented);
}

// Caching for http response abstraction.
// ----------------------------------------------------------------------------

// Cache request for serialization, keeping it out of dispatch.
void protocol_http::set_request(const request_cptr& request) NOEXCEPT
{
    ////BC_ASSERT(stranded());
    BC_ASSERT(request);
    request_ = request;
}

/// Returns default if not set, as ws frames and a tcp downgrade carry no http
// envelope (asserts correctness otherwise).
request_cptr protocol_http::reset_request() NOEXCEPT
{
    ////BC_ASSERT(stranded());
    BC_ASSERT(request_ || websocket() || downgraded());

    if (request_)
    {
        auto copy = request_;
        request_.reset();
        return copy;
    }

    return system::to_shared<request>();
}

const request_cptr& protocol_http::get_request() const NOEXCEPT
{
    ////BC_ASSERT(stranded());
    return request_;
}

response protocol_http::to_response(status status) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    response message{ status, request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    return message;
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_http::send_json(system::json::value&& model,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.set(field::content_type,
        from_media_type(media_type::application_json));
    message.body() = json_value
    {
        .model = std::move(model),
        .size_hint = size_hint
    };
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_text(std::string&& text) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.set(field::content_type, from_media_type(media_type::text_plain));
    message.body() = std::move(text);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_chunk(system::data_chunk&& bytes) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.set(field::content_type,
        from_media_type(media_type::application_octet_stream));
    message.body() = std::move(bytes);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_file(file&& file, media_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT_MSG(file.is_open(), "sending closed file handle");
    auto message = to_response(status::ok);
    message.set(field::content_type, from_media_type(type));
    message.body() = std::move(file);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_span(span_body::value_type&& span,
    media_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.set(field::content_type, from_media_type(type));
    message.body() = std::move(span);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_buffer(buffer_body::value_type&& buffer,
    media_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.set(field::content_type, from_media_type(type));
    message.body() = std::move(buffer);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_empty() NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.body() = empty_value{};
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_http::send_stream(size_t size, wire_writer&& writer,
    media_type type) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto message = to_response(status::ok);
    message.set(field::content_type, from_media_type(type));
    message.body() = stream_value
    {
        .write = std::move(writer),
        .size = size
    };
    message.prepare_payload();
    SEND(std::move(message), handle_stream, _1);
}

// Error senders.
// ----------------------------------------------------------------------------

void protocol_http::send_bad_request() NOEXCEPT
{
    send_bad_request(*reset_request());
}

void protocol_http::send_not_found() NOEXCEPT
{
    send_not_found(*reset_request());
}

void protocol_http::send_not_acceptable() NOEXCEPT
{
    send_not_acceptable(*reset_request());
}

void protocol_http::send_forbidden() NOEXCEPT
{
    send_forbidden(*reset_request());
}

void protocol_http::send_not_implemented() NOEXCEPT
{
    send_not_implemented(*reset_request());
}

void protocol_http::send_method_not_allowed() NOEXCEPT
{
    send_method_not_allowed(*reset_request());
}

void protocol_http::send_internal_server_error(const code& reason) NOEXCEPT
{
    send_internal_server_error(reason, *reset_request());
}

void protocol_http::send_bad_request(const request& request) NOEXCEPT
{
    network::protocol_http::send_bad_request(request);
}

void protocol_http::send_not_found(const request& request) NOEXCEPT
{
    network::protocol_http::send_not_found(request);
}

void protocol_http::send_not_acceptable(const request& request) NOEXCEPT
{
    network::protocol_http::send_not_acceptable(request);
}

void protocol_http::send_forbidden(const request& request) NOEXCEPT
{
    network::protocol_http::send_forbidden(request);
}

void protocol_http::send_not_implemented(const request& request) NOEXCEPT
{
    network::protocol_http::send_not_implemented(request);
}

void protocol_http::send_method_not_allowed(const request& request) NOEXCEPT
{
    network::protocol_http::send_method_not_allowed(request);
}

void protocol_http::send_internal_server_error(const code& reason,
    const request& request) NOEXCEPT
{
    network::protocol_http::send_internal_server_error(reason, request);
}

// A stream body write fails as unexpected only when its writer fails.
void protocol_http::handle_stream(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (ec == network::error::unexpected_body)
        fault(database::error::integrity);

    handle_complete(ec, network::error::success);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
