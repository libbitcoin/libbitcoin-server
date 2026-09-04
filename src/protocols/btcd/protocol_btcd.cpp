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
#include <bitcoin/server/protocols/protocol_btcd.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd
#define SUBSCRIBE_BTCD(method, ...) \
    btcd_subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_btcd::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    subscribe_chase(BIND(handle_chase, _1, _2, _3));

    // Administrative methods.
    SUBSCRIBE_BTCD(handle_authenticate, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_session, _1, _2);
    SUBSCRIBE_BTCD(handle_stop, _1, _2);

    // Getter methods.
    SUBSCRIBE_BTCD(handle_estimate_fee, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_get_best_block, _1, _2);
    SUBSCRIBE_BTCD(handle_get_block_chain_info, _1, _2);
    SUBSCRIBE_BTCD(handle_get_cfilter, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_get_cfilter_header, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_get_current_net, _1, _2);
    SUBSCRIBE_BTCD(handle_get_difficulty, _1, _2);
    SUBSCRIBE_BTCD(handle_get_headers, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_get_info, _1, _2);
    SUBSCRIBE_BTCD(handle_get_net_totals, _1, _2);
    SUBSCRIBE_BTCD(handle_search_raw_transactions, _1, _2, _3, _4, _5, _6, _7, _8, _9);
    SUBSCRIBE_BTCD(handle_version, _1, _2);

    // Subscription methods.
    SUBSCRIBE_BTCD(handle_notify_blocks, _1, _2);
    SUBSCRIBE_BTCD(handle_stop_notify_blocks, _1, _2);
    SUBSCRIBE_BTCD(handle_notify_new_transactions, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop_notify_new_transactions, _1, _2);

    // Filter methods.
    SUBSCRIBE_BTCD(handle_load_tx_filter, _1, _2, _3, _4, _5);
    SUBSCRIBE_BTCD(handle_rescan_blocks, _1, _2, _3);

    // Deprecated methods.
    SUBSCRIBE_BTCD(handle_notify_received, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop_notify_received, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_notify_spent, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_stop_notify_spent, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_rescan, _1, _2, _3, _4, _5, _6);

    // Publish served method names (e.g. for control subgroup help).
    register_methods(btcd_interface::names);

    // The bitcoind interface subgroups are independently attached.
    SUBSCRIBE_CHANNEL(post, handle_receive_post, _1, _2);
    SUBSCRIBE_CHANNEL(network::http::method::unknown, handle_receive_unknown,
        _1, _2);
    network::protocol::start();
}

// Events unsubscription is asynchronous, race is ok.
void protocol_btcd::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    btcd_dispatcher_.stop(ec);
    unsubscribe_chase();
    network::protocol_http::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

void protocol_btcd::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Silently defer invalidated requests to the terminal responder.
    if (!is_allowed_host(*post, post->version()) ||
        !is_allowed_origin(*post, post->version()) ||
        !post->body().contains<request>())
        return;

    const auto& message = post->body().get<request>().message;

    // authenticate is websocket-only (as btcd): excluded from the post
    // surface, silently deferring to the terminal miss (method not found).
    if (message.method == btcd_interface::authenticate::name)
        return;

    // Silently defer methods not defined by the btcd interface.
    if (!btcd_dispatcher::contains(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_error(error::btcd::invalid_params);
        return;
    }

    // Dispatch the request to the interface dispatcher.
    if (const auto code = btcd_dispatcher_.notify(message))
        stop(code);
}

void protocol_btcd::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Silently defer non-rpc frames to the terminal responder (which stops).
    if (!request.body().contains<rpc::request>())
        return;

    const auto& message = request.body().get<rpc::request>().message;
    const auto authenticate =
        (message.method == btcd_interface::authenticate::name);

    // In-band authorization: until authorized, every method (of any attached
    // interface) is unauthorized except authenticate, which is thereafter.
    // Claimed here so the connection stays open for the authentication flow.
    if (authenticate == authorized())
    {
        set_claimed();
        set_rpc_request(message);
        send_error(network::error::unauthorized);
        return;
    }

    // Silently defer methods not defined by the btcd interface.
    if (!btcd_dispatcher::contains(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();
    set_rpc_request(message);

    // The credential may be restricted to a subset of interface methods.
    if (!authenticate && !permitted(message.method))
    {
        send_error(error::btcd::invalid_params);
        return;
    }

    // Dispatch the request to the interface dispatcher.
    if (const auto code = btcd_dispatcher_.notify(message))
        stop(code);
}

// Handlers (administrative).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_authenticate(const code& ec,
    btcd_interface::authenticate, const std::string& username,
    const std::string& passphrase) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Reachable only when not authorized (see dispatch_websocket).
    set_authorized(network::config::credential::to_digest(username,
        passphrase));

    if (authorized())
    {
        send_result({}, 4);
        return true;
    }

    // Failed authentication ends session once error has reached caller.
    const code unauthorized{ network::error::unauthorized };
    const auto size = two * unauthorized.message().size();
    send_error(unauthorized, size, unauthorized);
    return true;
}

bool protocol_btcd::handle_session(const code& ec,
    btcd_interface::session) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t result{};
    result.emplace("id", identifier());
    send_result(std::move(result), 32);
    return true;
}

bool protocol_btcd::handle_stop(const code& ec,
    btcd_interface::stop) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The server/node cannot stop itself.
    send_error(error::btcd::unimplemented);
    return true;
}

// Handlers (subscription).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_notify_blocks(const code& ec,
    btcd_interface::notify_blocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_blocks_.store(true, relaxed);
    send_result({}, 4);
    return true;
}

bool protocol_btcd::handle_stop_notify_blocks(const code& ec,
    btcd_interface::stop_notify_blocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_blocks_.store(false, relaxed);
    send_result({}, 4);
    return true;
}

bool protocol_btcd::handle_notify_new_transactions(const code& ec,
    btcd_interface::notify_new_transactions, bool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::btcd::unimplemented);
    return true;
}

bool protocol_btcd::handle_stop_notify_new_transactions(const code& ec,
    btcd_interface::stop_notify_new_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::btcd::unimplemented);
    return true;
}

// Handlers (deprecated, not_implemented).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_notify_received(const code& ec,
    btcd_interface::notify_received, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::btcd::unimplemented);
    return true;
}

bool protocol_btcd::handle_stop_notify_received(const code& ec,
    btcd_interface::stop_notify_received, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::btcd::unimplemented);
    return true;
}

bool protocol_btcd::handle_notify_spent(const code& ec,
    btcd_interface::notify_spent, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::btcd::unimplemented);
    return true;
}

bool protocol_btcd::handle_stop_notify_spent(const code& ec,
    btcd_interface::stop_notify_spent, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::btcd::unimplemented);
    return true;
}

// Implemented only for the empty addresses/outpoints case (as btcd).
// This is the call btcwallet makes to bootstrap its sync starting point.
bool protocol_btcd::handle_rescan(const code& ec,
    btcd_interface::rescan, const std::string& beginblock,
    const value_t& addresses, const value_t& outpoints,
    const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;

    hash_digest begin_hash{};
    if (!decode_hash(begin_hash, beginblock))
    {
        send_error(error::btcd::invalid_address_or_key, two * beginblock.size());
        return true;
    }

    const auto& query = archive();
    if (query.to_header(begin_hash).is_terminal())
    {
        send_error(error::btcd::invalid_address_or_key, two * beginblock.size());
        return true;
    }

    const auto has_addresses = std::holds_alternative<array_t>(
        addresses.value()) && !std::get<array_t>(addresses.value()).empty();
    const auto has_outpoints = std::holds_alternative<array_t>(
        outpoints.value()) && !std::get<array_t>(outpoints.value()).empty();

    if (has_addresses || has_outpoints)
    {
        send_error(error::btcd::unimplemented);
        return true;
    }

    const auto top = query.get_top_confirmed();
    const auto confirmed = query.to_confirmed(top);
    const auto header = query.get_header(confirmed);
    if (!header)
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    send_result({}, 4);
    send_notification("rescanfinished", array_t
    {
        encode_hash(header->hash()),
        top,
        header->timestamp()
    }, 256);
    return true;
}

// Chase events.
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_chase(const code&, node::chase event_,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case node::chase::organized:
        {
            if (subscribed_blocks_.load(relaxed))
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST_NOTIFY(do_connected, std::get<node::header_t>(value));
            }
            break;
        }
        case node::chase::reorganized:
        {
            if (subscribed_blocks_.load(relaxed))
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST_NOTIFY(do_disconnected, std::get<node::header_t>(value));
            }
            break;
        }
        default:
        {
            break;
        }
    }

    return !stopped();
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_btcd::send_notification(const std::string& method,
    array_t&& params, size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());

    using namespace network::http;
    static const auto json = from_media_type(media_type::application_json);
    rpc::request notification{ { .size_hint = size_hint } };
    notification.message.jsonrpc = version::v1;
    notification.message.method = method;
    notification.message.params = params_t{ std::move(params) };

    http::response message{ status::ok, 11 };
    message.set(field::content_type, json);
    message.body() = std::move(notification);
    message.prepare_payload();

    // The notification strand poster shadows notify.
    this->network::protocol_http::template notify<CLASS>(std::move(message),
        &CLASS::handle_complete, _1, error::success);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
