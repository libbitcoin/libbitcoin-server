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
#include <bitcoin/server/protocols/protocol_btcd_rpc.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd_rpc
#define SUBSCRIBE_BTCD(method, ...) \
    btcd_subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

// protocol_bitcoind_rpc declares 'using post = network::http::method::post',
// which shadows network::protocol::post<Derived>. Qualify explicitly.
#define POST_BTCD(method, ...) \
    this->network::protocol::template post<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_btcd_rpc::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    subscribe_chase(BIND(handle_chase, _1, _2, _3));

    SUBSCRIBE_BTCD(handle_authenticate, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_session, _1, _2);
    SUBSCRIBE_BTCD(handle_notify_blocks, _1, _2);
    SUBSCRIBE_BTCD(handle_stop_notify_blocks, _1, _2);
    SUBSCRIBE_BTCD(handle_notify_new_transactions, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop_notify_new_transactions, _1, _2);
    SUBSCRIBE_BTCD(handle_load_tx_filter, _1, _2, _3, _4, _5);
    SUBSCRIBE_BTCD(handle_rescan_blocks, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop, _1, _2);
    SUBSCRIBE_BTCD(handle_notify_received, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop_notify_received, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_notify_spent, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop_notify_spent, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_rescan, _1, _2, _3, _4, _5, _6);

    // Base registers all standard chain rpc handlers and starts the listener.
    protocol_bitcoind_rpc::start();
}

void protocol_btcd_rpc::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    btcd_dispatcher_.stop(ec);
    unsubscribe_chase();
    protocol_bitcoind_rpc::stopping(ec);
}

// Websocket dispatch.
// ----------------------------------------------------------------------------
// btcd extension methods (session/notify/filter/admin) arrive as ws frames
// here. Standard chain methods (getblockcount etc.) remain reachable via
// plain http post on the same endpoint, unchanged, via the inherited
// handle_receive_post. They are NOT yet bridged into this ws dispatcher: that
// requires reconciling protocol_bitcoind_rpc's post-oriented private send
// path (which caches the original http::request for header derivation) with
// this class's ws-oriented senders. Tracked for phase B.

void protocol_btcd_rpc::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // channel_btcd::websocket_body() preselects the json-rpc request parser.
    if (!request.body().contains<rpc::request>())
    {
        send_btcd_error(error::invalid_argument);
        return;
    }

    const auto& body = request.body().get<rpc::request>();
    const auto& message = body.message;

    // Cache request context for response building (version + id).
    btcd_version_ = message.jsonrpc;
    btcd_id_ = message.id;

    // channel_btcd::unauthorized() bypasses the (structurally per-http-
    // request-only) basic auth check for the whole ws session, so when a
    // credential is configured, enforcement happens here instead: every
    // method other than 'authenticate' itself is rejected until it succeeds.
    if (options_.authorize() && !btcd_channel_->authenticated() &&
        message.method != btcd_interface::authenticate::name)
    {
        const code unauthorized{ network::error::unauthorized };
        send_btcd_error(unauthorized, two * unauthorized.message().size(),
            unauthorized);
        return;
    }

    const auto code = btcd_dispatcher_.notify(message);
    if (!code)
        return;

    // Unknown method: reply with a json-rpc error and keep the ws connection
    // alive (matches real btcd behavior, and the same guard already applied
    // to protocol_bitcoind_rpc::handle_receive_post for its own dispatcher).
    if (code == network::error::unexpected_method)
        send_btcd_error(code);
    else
        stop(code);
}

// Handlers (session/handshake).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_authenticate(const code& ec,
    btcd_interface::authenticate, const std::string& username,
    const std::string& password) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // No-op when no server-side credential is configured. Otherwise the
    // username/password must match the configured single-tier credential.
    if (!options_.authorize() ||
        (username == options_.username && password == options_.password))
    {
        btcd_channel_->set_authenticated(true);
        send_btcd_result({}, 4);
        return true;
    }

    // Close once the error response has actually been written, rather than
    // stopping synchronously here and racing the (async) send.
    const code unauthorized{ network::error::unauthorized };
    send_btcd_error(unauthorized, two * unauthorized.message().size(),
        unauthorized);
    return false;
}

bool protocol_btcd_rpc::handle_session(const code& ec,
    btcd_interface::session) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The channel identifier is unique per connection for the process
    // lifetime, satisfying btcd's reconnect-detection use of session ids.
    object_t result{};
    result.emplace("id", value_t{ possible_sign_cast<int64_t>(identifier()) });
    send_btcd_result(value_t{ std::move(result) }, 32);
    return true;
}

// Handlers (block subscription).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_notify_blocks(const code& ec,
    btcd_interface::notify_blocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_blocks_.store(true, std::memory_order_relaxed);
    send_btcd_result({}, 4);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_blocks(const code& ec,
    btcd_interface::stop_notify_blocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_blocks_.store(false, std::memory_order_relaxed);
    send_btcd_result({}, 4);
    return true;
}

// Handlers (mempool subscription, not_implemented pending v5 mempool).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_notify_new_transactions(const code& ec,
    btcd_interface::notify_new_transactions, bool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_new_transactions(const code& ec,
    btcd_interface::stop_notify_new_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

// Handlers (address/outpoint filtering, not_implemented pending phase B).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_load_tx_filter(const code& ec,
    btcd_interface::load_tx_filter, bool, const value_t&,
    const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_rescan_blocks(const code& ec,
    btcd_interface::rescan_blocks, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

// Handler (admin, permanently not_implemented).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_stop(const code& ec,
    btcd_interface::stop) NOEXCEPT
{
    if (stopped(ec)) return false;

    // No secure remote-shutdown path exists; always rejected regardless of
    // configuration or auth state.
    send_btcd_error(error::not_implemented);
    return true;
}

// Handlers (deprecated, permanently not_implemented).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_notify_received(const code& ec,
    btcd_interface::notify_received, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_received(const code& ec,
    btcd_interface::stop_notify_received, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_notify_spent(const code& ec,
    btcd_interface::notify_spent, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_spent(const code& ec,
    btcd_interface::stop_notify_spent, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_rescan(const code& ec,
    btcd_interface::rescan, const std::string&, const value_t&,
    const value_t&, const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_btcd_error(error::not_implemented);
    return true;
}

// Chase events.
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_chase(const code& ec, node::chase event_,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case node::chase::organized:
            if (subscribed_blocks_.load(std::memory_order_relaxed))
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST_BTCD(do_block_connected, std::get<node::header_t>(value));
            }
            break;

        case node::chase::reorganized:
            if (subscribed_blocks_.load(std::memory_order_relaxed))
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST_BTCD(do_block_disconnected, std::get<node::header_t>(value));
            }
            break;

        default:
            break;
    }

    return !stopped();
}

void protocol_btcd_rpc::do_block_connected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(std::memory_order_relaxed))
        return;

    const database::header_link link{ link_value };
    const auto& query = archive();

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    // btcd 'blockconnected' notification: [hash, height, time] (positional,
    // matching btcjson.BlockConnectedNtfn -- the unfiltered notification
    // fired by notifyblocks, distinct from loadtxfilter's filtered variant).
    array_t params{};
    params.emplace_back(value_t{ encode_hash(query.get_header_key(link)) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(height) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(
        header->timestamp()) });

    send_btcd_notification("blockconnected", std::move(params), 256);
}

void protocol_btcd_rpc::do_block_disconnected(
    node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(std::memory_order_relaxed))
        return;

    const database::header_link link{ link_value };
    const auto& query = archive();

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    array_t params{};
    params.emplace_back(value_t{ encode_hash(query.get_header_key(link)) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(height) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(
        header->timestamp()) });

    send_btcd_notification("blockdisconnected", std::move(params), 256);
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_btcd_rpc::send_btcd_result(value_option&& result,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_btcd_rpc(
    {
        .jsonrpc = btcd_version_,
        .id = btcd_id_,
        .result = std::move(result)
    }, size_hint);
}

void protocol_btcd_rpc::send_btcd_error(const code& ec) NOEXCEPT
{
    send_btcd_error(ec, two * ec.message().size());
}

void protocol_btcd_rpc::send_btcd_error(const code& ec,
    size_t size_hint) NOEXCEPT
{
    send_btcd_error(ec, size_hint, error::success);
}

void protocol_btcd_rpc::send_btcd_error(const code& ec, size_t size_hint,
    const code& close_reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_btcd_rpc(
    {
        .jsonrpc = btcd_version_,
        .id = btcd_id_,
        .error = result_t
        {
            .code = ec.value(),
            .message = ec.message()
        }
    }, size_hint, close_reason);
}

void protocol_btcd_rpc::send_btcd_notification(const std::string& method,
    array_t&& params, size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace network::http;
    static const auto json = from_media_type(media_type::application_json);

    // Server-push notification: json-rpc-v1 request shape, no id.
    rpc::request notification{ { .size_hint = size_hint } };
    notification.message.jsonrpc = version::v1;
    notification.message.method = method;
    notification.message.params = params_t{ std::move(params) };

    http::response message{ status::ok, 11 };
    message.set(field::content_type, json);
    message.body() = std::move(notification);
    message.prepare_payload();

    // NOTIFY does not restart the reader (the ws reader is already active,
    // independent of this unprompted push).
    NOTIFY(std::move(message), handle_complete, _1, error::success);
}

// private
void protocol_btcd_rpc::send_btcd_rpc(response_t&& model, size_t size_hint,
    const code& close_reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace network::http;
    static const auto json = from_media_type(media_type::application_json);

    btcd_id_.reset();
    btcd_version_ = version::undefined;

    http::response message{ status::ok, 11 };
    message.set(field::content_type, json);
    message.body() = rpc::response
    {
        { .size_hint = size_hint }, std::move(model),
    };
    message.prepare_payload();

    // SEND restarts the ws reader so the next client frame is accepted.
    // handle_complete only stops the channel if close_reason is truthy, and
    // only after this write actually completes -- never synchronously here.
    SEND(std::move(message), handle_complete, _1, close_reason);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
