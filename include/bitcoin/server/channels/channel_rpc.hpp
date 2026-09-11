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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_RPC_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_RPC_HPP

#include <bitcoin/server/channels/channel_http.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Universal json-rpc channel, served over tcp/s (by downgrade), http/s and
/// ws/s (by upgrade). Preselecting the json-rpc body is what implies the
/// downgrade detection, so a service channel derives this to speak json-rpc
/// on any of the three, adding only its own channel state. The channel
/// carries the json-rpc senders (the protocol carries the dispatcher).
class BCS_API channel_rpc
  : public channel_http
{
public:
    typedef std::shared_ptr<channel_rpc> ptr;
    using channel_http::channel_http;

    /// Senders, rpc version and identity added to responses (requires strand).
    /// -----------------------------------------------------------------------

    inline void send_code(const code& ec,
        network::result_handler&& handler) NOEXCEPT
    {
        send_error(
        {
            .code = ec.value(),
            .message = ec.message()
        }, std::move(handler));
    }

    inline void send_error(network::rpc::result_t&& error,
        network::result_handler&& handler) NOEXCEPT
    {
        BC_ASSERT(stranded());
        const auto hint = 2u * error.message.size();
        send_response(
        {
            .jsonrpc = version_,
            .id = identity_,
            .error = std::move(error)
        }, hint, std::move(handler));
    }

    inline void send_result(network::rpc::value_t&& result, size_t size_hint,
        network::result_handler&& handler) NOEXCEPT
    {
        BC_ASSERT(stranded());
        send_response(
        {
            .jsonrpc = version_,
            .id = identity_,
            .result = std::move(result)
        }, size_hint, std::move(handler));
    }

    /// A notification requires a full duplex transport (ws or downgrade).
    inline void send_notification(network::rpc::string_t&& method,
        network::rpc::params_t&& params, size_t size_hint,
        network::result_handler&& handler) NOEXCEPT
    {
        BC_ASSERT(stranded());

        if (!websocket() && !downgraded())
        {
            handler(network::error::success);
            return;
        }

        send_request(
        {
            .jsonrpc = version_,
            .method = std::move(method),
            .params = std::move(params)
        }, size_hint, std::move(handler));
    }

protected:
    /// Overridden to set the preselected reader body type.
    inline value_type default_body() const NOEXCEPT override
    {
        return to_body<network::rpc::request>();
    }

    /// Overridden to cache the json-rpc request context (version and
    /// identity) for the response, the dispatch itself is by verb.
    inline void dispatch(
        const network::http::request_ptr& request) NOEXCEPT override
    {
        BC_ASSERT(stranded());

        const auto& body = request->body();
        if (body.contains<network::rpc::request>())
        {
            const auto& message = body.get<network::rpc::request>().message;
            version_ = message.jsonrpc;
            identity_ = message.id;
        }

        channel_http::dispatch(request);
    }

private:
    // The socket writes the body alone on a full duplex transport (ws frame
    // or downgraded stream), and the full http response otherwise.
    inline void send_response(network::rpc::response_t&& model,
        size_t size_hint, network::result_handler&& handler) NOEXCEPT
    {
        using namespace network::http;
        response message{ status::ok, version_1_1 };
        message.set(field::content_type,
            from_media_type(media_type::application_json));
        message.body() = network::rpc::response
        {
            { .size_hint = size_hint }, std::move(model)
        };

        message.prepare_payload();
        send(std::move(message), std::move(handler));
    }

    inline void send_request(network::rpc::request_t&& model, size_t size_hint,
        network::result_handler&& handler) NOEXCEPT
    {
        using namespace network::http;
        response message{ status::ok, version_1_1 };
        message.body() = network::rpc::request
        {
            { .size_hint = size_hint }, std::move(model)
        };

        notify(std::move(message), std::move(handler));
    }

    // These are protected by strand.
    network::rpc::version version_{};
    network::rpc::id_option identity_{};
};

} // namespace server
} // namespace libbitcoin

#endif
