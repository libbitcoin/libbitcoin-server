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
#include <bitcoin/server/protocols/protocol_bitcoind_version.hpp>

#include <algorithm>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_version
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/finished (handshake).
// ----------------------------------------------------------------------------

// Session resumes the channel following return from start().
// Sends are not precluded, but no messages can be received while paused.
void protocol_bitcoind_version::shake(result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
    {
        handler(network::error::operation_failed);
        return;
    }

    handler_ = system::move_shared<result_handler>(std::move(handler));

    SUBSCRIBE_CHANNEL(http::method::get, handle_receive_get, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::head, handle_receive_head, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::put, handle_receive_put, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::delete_, handle_receive_delete, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::trace, handle_receive_trace, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::options, handle_receive_options, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::connect, handle_receive_connect, _1, _2);
    SUBSCRIBE_CHANNEL(http::method::unknown, handle_receive_unknown, _1, _2);
    SUBSCRIBE_BITCOIND(handle_negotiate_version, _1, _2, _3, _4);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

void protocol_bitcoind_version::finished(const code& ec,
    const code& shake) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Invoke handshake completion, error will result in stopped channel.
    if (handler_)
    {
        (*handler_)(shake);
        handler_.reset();
    }
}

// While shaking, the only send is the response to a negotiateversion opener,
// which completes the handshake (failed unless negotiated).
void protocol_bitcoind_version::handle_complete(const code& ec,
    const code& reason) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!handler_)
    {
        protocol_bitcoind_dispatch<rpc_interface>::handle_complete(ec, reason);
        return;
    }

    if (negotiated_)
        finished(ec, error::success);
    else if (reason)
        finished(ec, reason);
    else
        finished(ec, error::bitcoind::invalid_parameter);
}

void protocol_bitcoind_version::register_names() NOEXCEPT
{
}

// Openers.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_version::is_negotiation(
    const request_t& message) NOEXCEPT
{
    return message.method == rpc_interface::negotiate_version::name;
}

// The handshake is not subject to authorization or method permission, as it
// is not a method of the service.
void protocol_bitcoind_version::negotiate(const request_t& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    pause();
    set_claimed();
    if (const auto code = rpc_dispatcher_.notify(message))
        handle_dispatch_fault(code);
}

// Any other opener passes at the configured version, dispatched again once
// the service is attached.
void protocol_bitcoind_version::pass_opener() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!handler_)
        return;

    pause();
    channel_->set_opener();
    finished(error::success, error::success);
}

void protocol_bitcoind_version::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (handler_)
    {
        if (!is_allowed_host(*post, post->version()) ||
            !is_allowed_origin(*post, post->version()) ||
            !post->body().contains<rpc::request>() ||
            !is_negotiation(post->body().get<rpc::request>().message))
        {
            pass_opener();
            return;
        }

        const auto& message = post->body().get<rpc::request>().message;
        set_rpc_request(message.jsonrpc, message.id, post);
        negotiate(message);
        return;
    }

    protocol_bitcoind_dispatch<rpc_interface>::handle_receive_post(ec, post);
}

void protocol_bitcoind_version::dispatch_websocket(
    const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (handler_)
    {
        if (!request.body().contains<rpc::request>() ||
            !is_negotiation(request.body().get<rpc::request>().message))
        {
            pass_opener();
            return;
        }

        const auto& message = request.body().get<rpc::request>().message;
        set_rpc_request(message);
        negotiate(message);
        return;
    }

    protocol_bitcoind_dispatch<rpc_interface>::dispatch_websocket(request);
}

void protocol_bitcoind_version::handle_receive_get(const code& ec,
    const http::method::get::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_head(const code& ec,
    const http::method::head::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_put(const code& ec,
    const http::method::put::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_delete(const code& ec,
    const http::method::delete_::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_trace(const code& ec,
    const http::method::trace::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_options(const code& ec,
    const options_verb::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_connect(const code& ec,
    const http::method::connect::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

void protocol_bitcoind_version::handle_receive_unknown(const code& ec,
    const http::method::unknown::cptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!stopped(ec)) pass_opener();
}

// Handler.
// ----------------------------------------------------------------------------

// Interface version negotiation (libbitcoin extension), accepted only as the
// opener of the channel.
bool protocol_bitcoind_version::handle_negotiate_version(const code& ec,
    rpc_interface::negotiate_version, const std::optional<double>& minimum,
    const std::optional<double>& maximum) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!handler_)
    {
        send_error(error::bitcoind::invalid_request);
        return true;
    }

    uint32_t lower{}, upper{ max_uint32 };
    if ((minimum.has_value() && !to_integer(lower, minimum.value())) ||
        (maximum.has_value() && !to_integer(upper, maximum.value())))
    {
        const code invalid{ error::bitcoind::invalid_parameter };
        send_error(invalid, invalid);
        return true;
    }

    lower = std::max(lower, options().protocol_minimum);
    upper = std::min(upper, options().protocol_maximum);
    const auto floor = server::interface::bitcoind::version_floor(upper);
    if (floor == server::interface::bitcoind::version::v0_0 ||
        to_value(floor) < lower)
    {
        const code invalid{ error::bitcoind::invalid_parameter };
        send_error(invalid, invalid);
        return true;
    }

    channel_->set_version(floor);
    negotiated_ = true;
    send_result(to_value(floor));
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
