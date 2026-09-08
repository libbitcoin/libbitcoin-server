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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ELECTRUM_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ELECTRUM_HPP

#include <bitcoin/server/channels/channel_rpc.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for the electrum service (universal json-rpc). Carries the
/// negotiated protocol version and the interface dispatcher, as electrum
/// dispatches by method name.
class BCS_API channel_electrum
  : public channel_rpc,
    protected network::tracker<channel_electrum>
{
public:
    typedef std::shared_ptr<channel_electrum> ptr;
    using interface_t = server::interface::electrum;
    using options_t = settings::electrum_server;
    using dispatcher = network::rpc::dispatcher<interface_t>;

    inline channel_electrum(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : channel_rpc(log, socket, identifier, config, options),
        options_(options),
        network::tracker<channel_electrum>(log)
    {
    }

    /// Subscribe to request from client (requires strand).
    /// Event handler is always invoked on the channel strand.
    template <class Unused, class Handler>
    inline void subscribe(Handler&& handler) NOEXCEPT
    {
        BC_ASSERT(stranded());
        dispatcher_.subscribe(std::forward<Handler>(handler));
    }

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

    /// Properties.
    /// -----------------------------------------------------------------------

    inline void set_client(const std::string& name) NOEXCEPT
    {
        name_ = name;
    }

    inline const std::string& client() const NOEXCEPT
    {
        return name_;
    }

    inline void set_version(server::electrum::version version) NOEXCEPT
    {
        version_e_ = version;
    }

    inline server::electrum::version version() const NOEXCEPT
    {
        return version_e_;
    }

    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

protected:
    /// Overridden to dispatch the json-rpc message by method name. Electrum
    /// laxness (single value params) is tolerated, so the base is not called.
    inline void dispatch(
        const network::http::request_cptr& request) NOEXCEPT override
    {
        BC_ASSERT(stranded());

        const auto& body = request->body();
        if (!body.contains<network::rpc::request>())
        {
            stop(network::error::bad_stream);
            return;
        }

        // Electrum laxness (single value params) is allowed, btcd laxness
        // (batched v1) is not, as with the json-rpc channel.
        const auto& value = body.get<network::rpc::request>();
        if (value.lax_batch)
        {
            stop(network::error::jsonrpc_batch_requires_v2);
            return;
        }

        // Cache request context for response building (version + id).
        const auto& message = value.message;
        version_ = message.jsonrpc;
        identity_ = message.id;

        if (const auto ec = dispatcher_.notify(message))
            stop(ec);
    }

    inline void stopping(const code& ec) NOEXCEPT override
    {
        dispatcher_.stop(ec);
        channel_http::stopping(ec);
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

    // This is thread safe.
    const options_t& options_;

    // These are protected by strand.
    network::rpc::version version_{};
    network::rpc::id_option identity_{};
    server::electrum::version version_e_{ server::electrum::version::v0_0 };
    std::string name_{};
    dispatcher dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
