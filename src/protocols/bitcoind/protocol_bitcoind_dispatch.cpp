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
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define TEMPLATE template <typename Interface>
#define CLASS protocol_bitcoind_dispatch<Interface>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A notification of a query is not executed, as its reply is empty.
TEMPLATE
bool CLASS::is_ignored(const network::rpc::request_t& message) NOEXCEPT
{
    return (message.jsonrpc == network::rpc::version::v2) &&
        !message.id.has_value() &&
        !network::rpc::is_command<Interface::methods>(message.method);
}

// Subscribe to the post and websocket transports of the interface.
TEMPLATE
void CLASS::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Publish served method names (e.g. for control subgroup help).
    register_methods(Interface::names);

    using websocket = network::http::method::websocket;
    using stream = network::http::method::stream;
    subscribe_channel<CLASS, post>(&CLASS::handle_receive_post,
        std::placeholders::_1, std::placeholders::_2);
    subscribe_channel<CLASS, websocket>(&CLASS::handle_receive_websocket,
        std::placeholders::_1, std::placeholders::_2);
    subscribe_channel<CLASS, stream>(&CLASS::handle_receive_stream,
        std::placeholders::_1, std::placeholders::_2);
    network::protocol::start();
}

TEMPLATE
void CLASS::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rpc_dispatcher_.stop(ec);
    protocol_bitcoind::stopping(ec);
}

// The post transport of the interface subgroup.
TEMPLATE
void CLASS::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // A protocol attached earlier owns the method (overrides this one).
    if (claimed())
        return;

    // Silently defer invalidated requests to the terminal responder.
    if (!is_allowed_host(*post, post->version()) ||
        !is_allowed_origin(*post, post->version()) ||
        !post->body().template contains<network::rpc::request>())
        return;

    // Get the parsed json-rpc request object.
    const auto& message =
        post->body().template get<network::rpc::request>().message;

    // Silently defer methods not defined by the interface subgroup.
    if (!rpc_dispatcher::contains(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();

    // The post is saved off during asynchronous handling and used by the
    // senders to formulate response headers, isolating handlers from http.
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_forbidden();
        return;
    }

    if (is_ignored(message))
    {
        send_result({});
        return;
    }

    // Dispatch the request to the interface dispatcher.
    if (const auto code = rpc_dispatcher_.notify(message))
        stop(code);
}

// The websocket transport of the interface subgroup.
TEMPLATE
void CLASS::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // A protocol attached earlier owns the method (overrides this one).
    if (claimed())
        return;

    // ws frames carry no headers, so ws authorization is enforced here,
    // not by the channel. Unauthorized and non-rpc frames are silently
    // deferred to the terminal responder (which stops the channel).
    if (!authorized() ||
        !request.body().template contains<network::rpc::request>())
        return;

    const auto& message =
        request.body().template get<network::rpc::request>().message;

    // Silently defer methods not defined by the interface subgroup.
    if (!rpc_dispatcher::contains(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();

    // Cache request context for response building (version + id).
    set_rpc_request(message);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        stop(network::error::unauthorized);
        return;
    }

    if (is_ignored(message))
    {
        send_result({});
        return;
    }

    // Dispatch the request to the interface dispatcher.
    if (const auto code = rpc_dispatcher_.notify(message))
        stop(code);
}

BC_POP_WARNING()

#undef CLASS
#undef TEMPLATE

// Explicit instantiation of the subgroup dispatch protocols, isolating the
// dispatch metaprogramming to this translation unit.
template class protocol_bitcoind_dispatch<interface::bitcoind_blockchain>;
template class protocol_bitcoind_dispatch<interface::bitcoind_control>;
template class protocol_bitcoind_dispatch<interface::bitcoind_mining>;
template class protocol_bitcoind_dispatch<interface::bitcoind_network>;
template class protocol_bitcoind_dispatch<interface::bitcoind_notifications>;
template class protocol_bitcoind_dispatch<interface::bitcoind_test>;
template class protocol_bitcoind_dispatch<interface::bitcoind_transaction>;
template class protocol_bitcoind_dispatch<interface::bitcoind_utility>;
template class protocol_bitcoind_dispatch<interface::bitcoind_wallet>;

} // namespace server

// Explicit instantiation of the subgroup dispatchers, from a namespace
// enclosing network::rpc (as required and declared in the subgroup headers).
template class network::rpc::dispatcher<
    server::interface::bitcoind_blockchain>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_control>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_mining>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_network>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_notifications>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_test>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_transaction>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_utility>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_wallet>;

} // namespace libbitcoin
