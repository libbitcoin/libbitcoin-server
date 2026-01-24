/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_html.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_html
    
using namespace system;
using namespace network::http;
using namespace std::placeholders;

// Shared pointers required in handler parameters so closures control lifetime.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Handle get method.
// ----------------------------------------------------------------------------

void protocol_html::handle_receive_get(const code& ec,
    const method::get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http origin form for get.
    if (!is_origin_form(get->target()))
    {
        send_bad_target({}, *get);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*get, get->version()))
    {
        send_forbidden(*get);
        return;
    }

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*get, get->version()))
    {
        send_bad_host(*get);
        return;
    }

    // Always try API dispatch, false if unhandled.
    if (try_dispatch_object(*get))
        return;

    // Require file system dispatch if path is configured (always handles).
    if (!options_.path.empty())
    {
        dispatch_file(*get);
        return;
    }

    // Require embedded dispatch if site is configured (always handles).
    if (options_.pages.enabled())
    {
        dispatch_embedded(*get);
        return;
    }

    // Neither site is enabled and object dispatch doesn't support.
    send_not_implemented(*get);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_html::try_dispatch_object(const request&) NOEXCEPT
{
    return false;
}

void protocol_html::dispatch_embedded(const request& request) NOEXCEPT
{
    const auto& pages = server_settings().explore.pages;
    switch (const auto media = file_media_type(to_path(request.target()),
        media_type::text_html))
    {
        case media_type::text_html:
            send_span(pages.html(), media, request);
            break;
        case media_type::text_css:
            send_span(pages.css(), media, request);
            break;
        case media_type::application_javascript:
            send_span(pages.ecma(), media, request);
            break;
        case media_type::font_woff:
        case media_type::font_woff2:
            send_span(pages.font(), media, request);
            break;
        case media_type::image_png:
        case media_type::image_gif:
        case media_type::image_jpeg:
            send_span(pages.icon(), media, request);
            break;
        default:
            send_not_found(request);
    }
}

void protocol_html::dispatch_file(const request& request) NOEXCEPT
{
    // Empty path implies malformed target (terminal).
    auto path = to_local_path(request.target());
    if (path.empty())
    {
        send_bad_target({}, request);
        return;
    }

    // If no file extension it's REST on the single/default html page.
    if (!path.has_extension())
    {
        path = to_local_path();

        // Default html page (e.g. index.html) is not configured.
        if (path.empty())
        {
            send_not_implemented(request);
            return;
        }
    }

    // Get the single/default or explicitly requested page.
    auto file = get_file_body(path);
    if (!file.is_open())
    {
        send_not_found(request);
        return;
    }

    const auto octet_stream = media_type::application_octet_stream;
    send_file(std::move(file), file_media_type(path, octet_stream), request);
}

// Senders.
// ----------------------------------------------------------------------------

constexpr auto data = media_type::application_octet_stream;
constexpr auto json = media_type::application_json;
constexpr auto text = media_type::text_plain;

void protocol_html::send_json(boost::json::value&& model, size_t size_hint,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    response.set(field::content_type, from_media_type(json));
    response.body() = json_value{ std::move(model), size_hint };
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_text(std::string&& hexidecimal,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    response.set(field::content_type, from_media_type(text));
    response.body() = std::move(hexidecimal);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_chunk(system::data_chunk&& bytes,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    response.set(field::content_type, from_media_type(data));
    response.body() = std::move(bytes);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_file(file&& file, media_type type,
    const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    BC_ASSERT_MSG(file.is_open(), "sending closed file handle");
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    response.set(field::content_type, from_media_type(type));
    response.body() = std::move(file);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_span(span_body::value_type&& span,
    media_type type, const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    response.set(field::content_type, from_media_type(type));
    response.body() = std::move(span);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

void protocol_html::send_buffer(buffer_body::value_type&& buffer,
    media_type type, const request& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    response response{ status::ok, request.version() };
    add_common_headers(response, request);
    add_access_control_headers(response, request);
    response.set(field::content_type, from_media_type(type));
    response.body() = std::move(buffer);
    response.prepare_payload();
    SEND(std::move(response), handle_complete, _1, error::success);
}

// Utilities.
// ----------------------------------------------------------------------------

std::filesystem::path protocol_html::to_path(
    const std::string& target) const NOEXCEPT
{
    return target == "/" ? target + options_.default_ : target;
}

std::filesystem::path protocol_html::to_local_path(
    const std::string& target) const NOEXCEPT
{
    return sanitize_origin(options_.path, to_path(target).string());
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
