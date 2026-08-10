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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_DISPATCH_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_DISPATCH_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind.hpp>

namespace libbitcoin {
namespace server {

/// Interface subgroup dispatch, the common shape of the bitcoind subgroup
/// protocols. Carries the subgroup interface dispatcher and claims each
/// request defined by the interface, silently deferring all others to the
/// terminal responder (protocol_bitcoind, attached last). Each subgroup
/// explicitly instantiates this template (and its dispatcher) in its own
/// translation unit, isolating the dispatch metaprogramming there.
template <typename Interface>
class protocol_bitcoind_dispatch
  : public protocol_bitcoind
{
public:
    using rpc_dispatcher = network::rpc::dispatcher<Interface>;

    /// Subscribe to the post and websocket transports of the interface.
    void start() NOEXCEPT override
    {
        BC_ASSERT(stranded());

        if (started())
            return;

        // Publish served method names (e.g. for control subgroup help).
        register_methods(Interface::names);

        using unknown = network::http::method::unknown;
        subscribe_channel<protocol_bitcoind_dispatch<Interface>, post>(
            &protocol_bitcoind_dispatch<Interface>::handle_receive_post,
            std::placeholders::_1, std::placeholders::_2);
        subscribe_channel<protocol_bitcoind_dispatch<Interface>, unknown>(
            &protocol_bitcoind_dispatch<Interface>::handle_receive_unknown,
            std::placeholders::_1, std::placeholders::_2);
        network::protocol::start();
    }

    void stopping(const code& ec) NOEXCEPT override
    {
        BC_ASSERT(stranded());
        rpc_dispatcher_.stop(ec);
        network::protocol_http::stopping(ec);
    }

protected:
    inline protocol_bitcoind_dispatch(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind(session, channel, options)
    {
    }

    /// The post transport of the interface subgroup.
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override
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

    /// The websocket transport of the interface subgroup.
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override
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

    /// Subgroup handler wiring (dispatcher subscription).
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        rpc_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // This is protected by strand.
    rpc_dispatcher rpc_dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
