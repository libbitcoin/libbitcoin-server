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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_RPC_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_RPC_HPP

#include <string_view>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>

namespace libbitcoin {
namespace server {

/// Universal json-rpc service protocol, the common shape of a service (and
/// its derivations) over channel_rpc. Carries the interface dispatcher and
/// claims each request defined by the interface, deferring all others to the
/// protocol attached last (see handle_unclaimed). Protocols attached to one
/// channel (e.g. a handshake) therefore publish disjoint interfaces. The
/// senders are forwarded to the channel, so a service is served identically
/// over tcp/s (by downgrade), http/s and ws/s (by upgrade).
template <typename Interface>
class protocol_rpc
  : public server::protocol_http
{
public:
    using rpc_dispatcher = network::rpc::dispatcher<Interface>;

    /// Subscribe to the post and websocket transports of the interface.
    void start() NOEXCEPT override
    {
        BC_ASSERT(stranded());
        if (started())
            return;

        using self = protocol_rpc<Interface>;
        using post = network::http::method::post;
        using unknown = network::http::method::unknown;
        using namespace std::placeholders;

        // Publish served method names (e.g. for help).
        register_methods(names);
        subscribe_channel<self, post>(&self::handle_receive_post, _1, _2);
        subscribe_channel<self, unknown>(&self::handle_receive_unknown, _1, _2);
        network::protocol::start();
    }

    void stopping(const code& ec) NOEXCEPT override
    {
        BC_ASSERT(stranded());
        rpc_dispatcher_.stop(ec);
        network::protocol_http::stopping(ec);
    }

protected:
    inline protocol_rpc(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_http(session, channel, options),
        channel_(std::dynamic_pointer_cast<channel_rpc>(channel))
    {
    }

    /// The post transport of the interface.
    void handle_receive_post(const code& ec,
        const network::http::method::post::cptr& post) NOEXCEPT override
    {
        BC_ASSERT(stranded());
        if (stopped(ec))
            return;

        // A protocol attached earlier owns the method (overrides this one).
        if (claimed())
            return;

        // Silently defer invalidated requests (to the protocol attached last).
        if (!is_allowed_host(*post, post->version()) ||
            !is_allowed_origin(*post, post->version()) ||
            !post->body().template contains<network::rpc::request>())
            return;

        const auto& message =
            post->body().template get<network::rpc::request>().message;

        // Defer methods not defined by the interface.
        if (!rpc_dispatcher::contains(message.method))
        {
            handle_unclaimed(message);
            return;
        }

        // Claim the request (informs the protocol attached last).
        set_claimed();

        // The credential may be restricted to a subset of interface methods.
        if (!permitted(message.method))
        {
            send_forbidden(*post);
            return;
        }

        if (const auto code = rpc_dispatcher_.notify(message))
            stop(code);
    }

    /// The websocket (and downgrade) transport of the interface.
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override
    {
        BC_ASSERT(stranded());

        // A protocol attached earlier owns the method (overrides this one).
        if (claimed())
            return;

        // ws frames carry no headers, so ws authorization is enforced here.
        if (!authorized() ||
            !request.body().template contains<network::rpc::request>())
            return;

        const auto& message =
            request.body().template get<network::rpc::request>().message;

        // Defer methods not defined by the interface.
        if (!rpc_dispatcher::contains(message.method))
        {
            handle_unclaimed(message);
            return;
        }

        // Claim the request (informs the protocol attached last).
        set_claimed();

        // The credential may be restricted to a subset of interface methods.
        if (!permitted(message.method))
        {
            stop(network::error::unauthorized);
            return;
        }

        if (const auto code = rpc_dispatcher_.notify(message))
            stop(code);
    }

    /// Invoked for a request not defined by the interface, which the protocol
    /// attached last overrides to respond (all others defer by default).
    virtual void handle_unclaimed(
        const network::rpc::request_t&) NOEXCEPT
    {
    }

    /// Handler wiring (dispatcher subscription).
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        rpc_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    /// Senders, default completion (requires strand).
    /// -----------------------------------------------------------------------

    inline void send_code(const code& ec) NOEXCEPT
    {
        send_code(ec, default_handler());
    }

    inline void send_error(network::rpc::result_t&& error) NOEXCEPT
    {
        send_error(std::move(error), default_handler());
    }

    inline void send_result(network::rpc::value_t&& result,
        size_t size_hint) NOEXCEPT
    {
        send_result(std::move(result), size_hint, default_handler());
    }

    inline void send_notification(network::rpc::string_t&& method,
        network::rpc::params_t&& params, size_t size_hint) NOEXCEPT
    {
        send_notification(std::move(method), std::move(params), size_hint,
            default_handler());
    }

    /// Senders, caller completion (requires strand).
    /// -----------------------------------------------------------------------

    inline void send_code(const code& ec,
        network::result_handler&& handler) NOEXCEPT
    {
        channel_->send_code(ec, std::move(handler));
    }

    inline void send_error(network::rpc::result_t&& error,
        network::result_handler&& handler) NOEXCEPT
    {
        channel_->send_error(std::move(error), std::move(handler));
    }

    inline void send_result(network::rpc::value_t&& result, size_t size_hint,
        network::result_handler&& handler) NOEXCEPT
    {
        channel_->send_result(std::move(result), size_hint,
            std::move(handler));
    }

    inline void send_notification(network::rpc::string_t&& method,
        network::rpc::params_t&& params, size_t size_hint,
        network::result_handler&& handler) NOEXCEPT
    {
        channel_->send_notification(std::move(method), std::move(params),
            size_hint, std::move(handler));
    }

private:
    // The implemented method names of the interface.
    static constexpr auto name_data =
        network::rpc::method_names<Interface::methods>();
    static constexpr std::string_view names{ name_data.data(),
        name_data.size() };

    inline network::result_handler default_handler() NOEXCEPT
    {
        using self = protocol_rpc<Interface>;
        return std::bind(&self::handle_send,
            shared_from_base<self>(), std::placeholders::_1);
    }

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_rpc::ptr channel_;

    // This is protected by strand.
    rpc_dispatcher rpc_dispatcher_{};
};

/// Dispatcher subscription for the rpc interface of the CLASS.
#define SUBSCRIBE_RPC(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

} // namespace server
} // namespace libbitcoin

#endif
