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

    subscribe_chase(BIND(handle_chase, _1, _2));

    // Administrative methods.
    SUBSCRIBE_BTCD(handle_authenticate, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_session, _1, _2);

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
    SUBSCRIBE_BTCD(handle_node, _1, _2, _3, _4, _5);
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

    using websocket = network::http::method::websocket;
    using stream = network::http::method::stream;
    SUBSCRIBE_CHANNEL(websocket, handle_receive_websocket, _1, _2);
    SUBSCRIBE_CHANNEL(stream, handle_receive_stream, _1, _2);
    network::protocol::start();
}

// Events unsubscription is asynchronous, race is ok.
void protocol_btcd::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    btcd_dispatcher_.stop(ec);
    unsubscribe_chase();
    protocol_bitcoind::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------
// Dispatch precedes the bitcoind interface subgroups, so protocol rules applied
// here govern all methods of the channel.

// A request without an id (or with a null id) is neither executed nor answered.
static bool is_unanswered(const request_t& message) NOEXCEPT
{
    return !message.id.has_value() ||
        std::holds_alternative<null_t>(message.id.value());
}

static bool is_websocket_only(const std::string& method) NOEXCEPT
{
    static constexpr std::array methods
    {
        interface::btcd::authenticate::name,
        interface::btcd::session::name,
        interface::btcd::notify_blocks::name,
        interface::btcd::stop_notify_blocks::name,
        interface::btcd::notify_new_transactions::name,
        interface::btcd::stop_notify_new_transactions::name,
        interface::btcd::load_tx_filter::name,
        interface::btcd::rescan_blocks::name,
        interface::btcd::notify_received::name,
        interface::btcd::stop_notify_received::name,
        interface::btcd::notify_spent::name,
        interface::btcd::stop_notify_spent::name,
        interface::btcd::rescan::name
    };

    return std::ranges::find(methods, method) != methods.end();
}

void protocol_btcd::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec) || claimed())
        return;

    // Silently defer invalidated requests to the terminal responder.
    if (!is_allowed_host(*post, post->version()) ||
        !is_allowed_origin(*post, post->version()) ||
        !post->body().contains<request>())
        return;

    const auto& message = post->body().get<request>().message;
    if (is_unanswered(message))
    {
        set_claimed();
        set_rpc_request(version::v2, {}, post);
        send_result({});
        return;
    }

    // The credential may be restricted to a subset of channel methods.
    if (!permitted(message.method))
    {
        set_claimed();
        set_rpc_request(message.jsonrpc, message.id, post);
        send_error(error::btcd::internal_error);
        return;
    }

    // Websocket-only methods silently defer to the terminal miss.
    if (is_websocket_only(message.method))
        return;

    // Silently defer methods not defined by the btcd interface.
    if (!btcd_dispatcher::contains(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();
    set_rpc_request(message.jsonrpc, message.id, post);

    // Dispatch the request to the interface dispatcher.
    if (const auto code = btcd_dispatcher_.notify(message))
        handle_dispatch_fault(code);
}

void protocol_btcd::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Silently defer non-rpc frames to the terminal responder (which stops).
    if (claimed() || !request.body().contains<rpc::request>())
        return;

    const auto& message = request.body().get<rpc::request>().message;
    const auto authenticate =
        (message.method == btcd_interface::authenticate::name);

    // In-band authorization: until authorized, every method (of any attached
    // interface) is unauthorized except authenticate, which is thereafter.
    // Claimed here so the connection stays open for the authentication flow.
    // A repeated authenticate ends the connection once the error is sent.
    if (authenticate == authorized())
    {
        set_claimed();
        set_rpc_request(message);
        const code unauthorized{ network::error::unauthorized };
        if (authenticate)
            send_error(unauthorized, unauthorized);
        else
            send_error(unauthorized);

        return;
    }

    if (is_unanswered(message))
    {
        set_claimed();
        set_rpc_request(request_t{ .jsonrpc = version::v2 });
        send_result({});
        return;
    }

    // The credential may be restricted to a subset of channel methods.
    if (!authenticate && !permitted(message.method))
    {
        set_claimed();
        set_rpc_request(message);
        send_error(error::btcd::invalid_params);
        return;
    }

    // Silently defer methods not defined by the btcd interface.
    if (!btcd_dispatcher::contains(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();
    set_rpc_request(message);

    // Dispatch the request to the interface dispatcher.
    if (const auto code = btcd_dispatcher_.notify(message))
        handle_dispatch_fault(code);
}

code protocol_btcd::to_parameter_error(const code& ec) const NOEXCEPT
{
    return error::btcd::parameter_error(ec);
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
        send_result({});
        return true;
    }

    // Failed authentication ends session once error has reached caller.
    const code unauthorized{ network::error::unauthorized };
    send_error(unauthorized, unauthorized);
    return true;
}

bool protocol_btcd::handle_session(const code& ec,
    btcd_interface::session) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t result{};
    result.emplace("sessionid", identifier());
    send_result(std::move(result));
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
    send_result({});
    return true;
}

bool protocol_btcd::handle_stop_notify_blocks(const code& ec,
    btcd_interface::stop_notify_blocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_blocks_.store(false, relaxed);
    send_result({});
    return true;
}

bool protocol_btcd::handle_notify_new_transactions(const code& ec,
    btcd_interface::notify_new_transactions, bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return false;

    verbose_transactions_.store(verbose, relaxed);
    subscribed_transactions_.store(true, relaxed);
    send_result({});
    return true;
}

bool protocol_btcd::handle_stop_notify_new_transactions(const code& ec,
    btcd_interface::stop_notify_new_transactions) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_transactions_.store(false, relaxed);
    send_result({});
    return true;
}

// Handlers (deprecated).
// ----------------------------------------------------------------------------
// notify_received/notify_spent bodies live in protocol_btcd_filter.cpp.

// Implemented only for the empty addresses/outpoints case.
// This is the call btcwallet makes to bootstrap its sync starting point.
bool protocol_btcd::handle_rescan(const code& ec,
    btcd_interface::rescan, const std::string& beginblock,
    const std::optional<value_t>& addresses,
    const std::optional<value_t>& outpoints,
    const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;

    hash_digest begin_hash{};
    if (!decode_hash(begin_hash, beginblock))
    {
        send_error(error::btcd::deserialization_error);
        return true;
    }

    const auto& query = archive();
    if (query.to_header(begin_hash).is_terminal())
    {
        send_error(error::btcd::invalid_address_or_key);
        return true;
    }

    const auto has_entries = [](const std::optional<value_t>& list) NOEXCEPT
    {
        return list.has_value() &&
            std::holds_alternative<array_t>(list->value()) &&
            !std::get<array_t>(list->value()).empty();
    };

    const auto has_addresses = has_entries(addresses);
    const auto has_outpoints = has_entries(outpoints);

    if (has_addresses || has_outpoints)
    {
        send_error(error::btcd::unimplemented);
        return true;
    }

    size_t top{};
    const auto confirmed = query.to_top_confirmed();
    const auto header = query.get_header(confirmed);
    if (!header || !query.get_height(top, confirmed))
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    send_result({});
    send_notification("rescanfinished", array_t
    {
        encode_hash(header->hash()),
        to64(top),
        header->timestamp()
    });
    return true;
}

// Chase events.
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_chase(const code&, node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (stopped())
        return false;

    switch (node::to_chase(value))
    {
        case node::chase::organized:
        {
            if (subscribed_blocks_.load(relaxed) || watching_legacy_.load(relaxed))
            {
                POST_NOTIFY(do_connected,
                    node::to_payload<node::chase::organized>(value).link);
            }

            break;
        }
        case node::chase::reorganized:
        {
            if (subscribed_blocks_.load(relaxed) || watching_legacy_.load(relaxed))
            {
                POST_NOTIFY(do_disconnected,
                    node::to_payload<node::chase::reorganized>(value).link);
            }

            break;
        }
        case node::chase::transaction:
        {
            if (subscribed_transactions_.load(relaxed))
            {
                POST_NOTIFY(do_transaction,
                    node::to_payload<node::chase::transaction>(value).link);
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
    array_t&& params) NOEXCEPT
{
    BC_ASSERT(stranded());

    using namespace network::http;
    static const auto json = from_media_type(media_type::application_json);
    rpc::request notification{};
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
