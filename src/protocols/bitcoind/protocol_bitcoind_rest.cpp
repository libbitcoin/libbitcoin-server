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
#include <bitcoin/server/protocols/protocol_bitcoind_rest.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/system/chain/json/json.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_rest
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network::rpc;
using namespace network::http;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    protocol_bitcoind_rpc::start();
}

void protocol_bitcoind_rest::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rest_dispatcher_.stop(ec);
    protocol_bitcoind_rpc::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

// Base rpc protocol handles options and post, this derived handles get.
void protocol_bitcoind_rest::handle_receive_get(const code& ec,
    const get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*get, get->version()))
    {
        send_bad_host(*get);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*get, get->version()))
    {
        send_forbidden(*get);
        return;
    }

    // The get is saved off during asynchonous handling and used in send_json
    // to formulate response headers, isolating handlers from http semantics.
    set_request(get);

    // Parse the REST url into a json-rpc model and dispatch to a handler.
    request_t model{};
    if (bitcoind_target(model, get->target()))
    {
        send_not_found();
        return;
    }

    if (rest_dispatcher_.notify(model))
        send_not_found();
}

// Handlers.
// ----------------------------------------------------------------------------

constexpr auto data = to_value(media_type::application_octet_stream);
constexpr auto json = to_value(media_type::application_json);
constexpr auto text = to_value(media_type::text_plain);

template <typename Object, typename ...Args>
data_chunk to_bin(const Object& object, size_t size, Args&&... args) NOEXCEPT
{
    data_chunk out(size);
    stream::out::fast sink{ out };
    write::bytes::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    return out;
}

template <typename Object, typename ...Args>
std::string to_hex(const Object& object, size_t size, Args&&... args) NOEXCEPT
{
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    return out;
}

bool protocol_bitcoind_rest::handle_get_block(const code& ec,
    rest_interface::block, uint8_t media, system::hash_cptr hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!hash)
    {
        send_not_found();
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto block = query.get_block(query.to_header(*hash), witness);
    if (is_null(block))
    {
        send_not_found();
        return true;
    }

    const auto size = block->serialized_size(witness);
    switch (media)
    {
        case data:
            send_data(to_bin(*block, size, witness));
            return true;
        case text:
            send_hex(to_hex(*block, size, witness));
            return true;
        case json:
            send_dom(value_from(bitcoind_verbose(*block)), two * size);
            return true;
    }

    send_not_found();
    return true;
}

// Raw-http response senders (mirror protocol_html, not json-rpc enveloped).
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::send_data(data_chunk&& bytes) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    network::http::response message{ network::http::status::ok,
        request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(network::http::field::content_type, network::http::
        from_media_type(media_type::application_octet_stream));
    message.body() = std::move(bytes);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_bitcoind_rest::send_hex(std::string&& text) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    network::http::response message{ network::http::status::ok,
        request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(network::http::field::content_type, network::http::
        from_media_type(media_type::text_plain));
    message.body() = std::move(text);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_bitcoind_rest::send_dom(boost::json::value&& model,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    network::http::response message{ network::http::status::ok,
        request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(network::http::field::content_type, network::http::
        from_media_type(media_type::application_json));
    message.body() = network::http::json_value
    {
        .model = std::move(model),
        .size_hint = size_hint
    };
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

// private
// ----------------------------------------------------------------------------

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
