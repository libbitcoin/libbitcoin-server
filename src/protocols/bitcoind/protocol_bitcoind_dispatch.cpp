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

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define TEMPLATE template <typename Interface>
#define CLASS protocol_bitcoind_dispatch<Interface>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Subscribe to the post and websocket transports of the interface.
TEMPLATE
void CLASS::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Publish served method names (e.g. for control subgroup help).
    register_methods(Interface::names);

    using unknown = network::http::method::unknown;
    subscribe_channel<CLASS, post>(&CLASS::handle_receive_post,
        std::placeholders::_1, std::placeholders::_2);
    subscribe_channel<CLASS, unknown>(&CLASS::handle_receive_unknown,
        std::placeholders::_1, std::placeholders::_2);
    network::protocol::start();
}

TEMPLATE
void CLASS::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rpc_dispatcher_.stop(ec);
    network::protocol_http::stopping(ec);
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

    // The post is saved off during asynchonous handling and used in
    // send_json to formulate response headers, isolating handlers from
    // http semantics.
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_error(error::method_unauthorized);
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
        send_error(error::method_unauthorized);
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
