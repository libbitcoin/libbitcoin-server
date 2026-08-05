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
    SUBSCRIBE_BTCD(handle_get_current_net, _1, _2);
    SUBSCRIBE_BTCD(handle_get_best_block, _1, _2);
    SUBSCRIBE_BTCD(handle_get_difficulty, _1, _2);
    SUBSCRIBE_BTCD(handle_get_info, _1, _2);
    SUBSCRIBE_BTCD(handle_get_net_totals, _1, _2);
    SUBSCRIBE_BTCD(handle_get_network_hash_ps, _1, _2, _3, _4);
    SUBSCRIBE_BTCD(handle_create_raw_transaction, _1, _2, _3, _4, _5);
    SUBSCRIBE_BTCD(handle_decode_raw_transaction, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_decode_script, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_validate_address, _1, _2, _3);
    SUBSCRIBE_BTCD(handle_help, _1, _2, _3);
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

    // Base registers the bitcoind interface handlers and starts the listener.
    protocol_bitcoind_rpc::start();
}

void protocol_btcd_rpc::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    btcd_dispatcher_.stop(ec);
    unsubscribe_chase();
    protocol_bitcoind_rpc::stopping(ec);
}

// Dispatch (the post and websocket transports of the btcd interface).
// ----------------------------------------------------------------------------
// ws authorization is established at most once per connection, by basic auth
// on the upgrade request or by exactly one authenticate call -- so a ws call
// is invalid if authorized and the method is authenticate, or not authorized
// and the method is not authenticate (as btcd).

void protocol_btcd_rpc::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*post, post->version()))
    {
        send_bad_host(*post);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*post, post->version()))
    {
        send_forbidden(*post);
        return;
    }

    // Endpoint accepts only json-rpc posts.
    if (!post->body().contains<request>())
    {
        send_bad_request(*post);
        return;
    }

    const auto& message = post->body().get<request>().message;
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_error(error::unauthorized);
        return;
    }

    // authenticate is websocket-only (as btcd): excluded from the post
    // surface, deferring to the base miss (method not found).
    const auto code = (message.method == btcd_interface::authenticate::name) ?
        network::error::unexpected_method : btcd_dispatcher_.notify(message);

    // Defer to the base post transport on a method name miss.
    if (code == network::error::unexpected_method)
    {
        protocol_bitcoind_rpc::handle_receive_post(ec, post);
        return;
    }

    if (code)
        stop(code);
}

void protocol_btcd_rpc::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Websocket frames are parsed as json-rpc requests (websocket_rpc).
    if (!request.body().contains<rpc::request>())
    {
        stop(error::invalid_argument);
        return;
    }

    const auto& message = request.body().get<rpc::request>().message;

    // Cache request context for response building (version + id).
    set_rpc_request(message);

    const auto authenticate =
        (message.method == btcd_interface::authenticate::name);

    if (authenticate == authorized())
    {
        send_error(network::error::unauthorized);
        return;
    }

    if (!authenticate && !permitted(message.method))
    {
        send_error(network::error::unauthorized);
        return;
    }

    const auto code = btcd_dispatcher_.notify(message);

    // Defer to the base websocket transport on a method name miss.
    if (code == network::error::unexpected_method)
    {
        protocol_bitcoind_rpc::dispatch_websocket(request);
        return;
    }

    if (code)
        stop(code);
}

// Handlers (authentication/admin).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_authenticate(const code& ec,
    btcd_interface::authenticate, const std::string& username,
    const std::string& password) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Reachable only when not authorized (see dispatch_websocket). The
    // channel latches the digest only if it matches a configured credential.
    set_authorized(network::config::credential::to_digest(username, password));
    if (authorized())
    {
        send_result({}, 4);
        return true;
    }

    // A failed authenticate ends the session (as btcd), once the error has
    // reached the client.
    const code unauthorized{ network::error::unauthorized };
    send_error(unauthorized, two * unauthorized.message().size(),
        unauthorized);
    return true;
}

bool protocol_btcd_rpc::handle_session(const code& ec,
    btcd_interface::session) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The channel identifier is unique per connection for the process
    // lifetime, satisfying btcwallet's reconnect-detection use of session ids.
    object_t result{};
    result.emplace("id", value_t{ possible_sign_cast<int64_t>(identifier()) });
    send_result(value_t{ std::move(result) }, 32);
    return true;
}

bool protocol_btcd_rpc::handle_get_current_net(const code& ec,
    btcd_interface::get_current_net) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The p2p handshake magic, the same value btcd returns (checked once by
    // btcwallet/lnd at connect to confirm the expected network).
    send_result(
        value_t{ possible_sign_cast<int64_t>(network_settings().identifier) },
        20);
    return true;
}

bool protocol_btcd_rpc::handle_get_best_block(const code& ec,
    btcd_interface::get_best_block) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Required by btcwallet during wallet chain-sync bootstrap.
    const auto& query = archive();
    object_t result{};
    result.emplace("hash", value_t{ encode_hash(query.get_top_confirmed_hash()) });
    result.emplace("height", value_t{
        possible_sign_cast<int64_t>(query.get_top_confirmed()) });
    send_result(value_t{ std::move(result) }, 96);
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
    send_result({}, 4);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_blocks(const code& ec,
    btcd_interface::stop_notify_blocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    subscribed_blocks_.store(false, std::memory_order_relaxed);
    send_result({}, 4);
    return true;
}

// Handlers (mempool subscription, not_implemented pending v5 mempool).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_notify_new_transactions(const code& ec,
    btcd_interface::notify_new_transactions, bool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_new_transactions(const code& ec,
    btcd_interface::stop_notify_new_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

// Handlers (address/outpoint filtering): see protocol_btcd_rpc_filter.cpp.
// ----------------------------------------------------------------------------

// Handler (admin, permanently not_implemented).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_stop(const code& ec,
    btcd_interface::stop) NOEXCEPT
{
    if (stopped(ec)) return false;

    // No secure remote-shutdown path exists; always rejected regardless of
    // configuration or auth state.
    send_error(error::not_implemented);
    return true;
}

// Handlers (deprecated, permanently not_implemented).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_notify_received(const code& ec,
    btcd_interface::notify_received, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_received(const code& ec,
    btcd_interface::stop_notify_received, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_notify_spent(const code& ec,
    btcd_interface::notify_spent, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_stop_notify_spent(const code& ec,
    btcd_interface::stop_notify_spent, const value_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_btcd_rpc::handle_rescan(const code& ec,
    btcd_interface::rescan, const std::string& beginblock,
    const value_t& addresses, const value_t& outpoints,
    const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;

    // Deprecated method, implemented only for the empty addrs/outpoints
    // case (btcd skips scanning and reports immediate completion) -- the
    // call btcwallet makes to bootstrap its initial sync starting point. A
    // non-empty list would require a real historical scan; no observed
    // caller needs it (loadtxfilter/rescanblocks cover actual watching).
    hash_digest begin_hash{};
    if (!decode_hash(begin_hash, beginblock))
    {
        send_error(error::not_found, two * beginblock.size());
        return true;
    }

    const auto& query = archive();
    if (query.to_header(begin_hash).is_terminal())
    {
        send_error(error::not_found, two * beginblock.size());
        return true;
    }

    const auto has_addresses = std::holds_alternative<array_t>(
        addresses.value()) && !std::get<array_t>(addresses.value()).empty();
    const auto has_outpoints = std::holds_alternative<array_t>(
        outpoints.value()) && !std::get<array_t>(outpoints.value()).empty();

    if (has_addresses || has_outpoints)
    {
        send_error(error::not_implemented);
        return true;
    }

    // Nothing to watch: report the current chain tip as already-finished,
    // ignoring beginblock (as btcd).
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(error::not_found);
        return true;
    }

    // Reply to the rescan call itself before the unprompted notification.
    send_result({}, 4);

    array_t params{};
    params.emplace_back(value_t{ encode_hash(query.get_top_confirmed_hash()) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(top) });
    params.emplace_back(value_t{
        possible_sign_cast<int64_t>(header->timestamp()) });
    send_notification("rescanfinished", std::move(params), 256);
    return true;
}

// Chase events.
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_chase(const code&, node::chase event_,
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

    // btcd 'blockconnected' notification: [hash, height, time].
    array_t params{};
    params.emplace_back(value_t{ encode_hash(query.get_header_key(link)) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(height) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(
        header->timestamp()) });

    send_notification("blockconnected", std::move(params), 256);

    // btcd 'filteredblockconnected': [height, header, subscribedtxs]. Sent
    // unconditionally alongside blockconnected (as btcd) -- an empty/never-
    // loaded filter just yields an empty subscribedtxs array.
    constexpr auto witness = true;
    const auto block = query.get_block(link, witness);
    if (!block)
        return;

    array_t filtered_params{};
    filtered_params.emplace_back(value_t{
        possible_sign_cast<int64_t>(height) });
    filtered_params.emplace_back(value_t{
        to_text(*header, chain::header::serialized_size()) });
    filtered_params.emplace_back(value_t{ match_filtered_transactions(*block) });

    send_notification("filteredblockconnected",
        std::move(filtered_params), 256);
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

    send_notification("blockdisconnected", std::move(params), 256);

    // btcd 'filteredblockdisconnected': [height, header].
    array_t filtered_params{};
    filtered_params.emplace_back(value_t{
        possible_sign_cast<int64_t>(height) });
    filtered_params.emplace_back(value_t{
        to_text(*header, chain::header::serialized_size()) });

    send_notification("filteredblockdisconnected",
        std::move(filtered_params), 256);
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_btcd_rpc::send_notification(const std::string& method,
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

    // NOTIFY does not restart the (already active) ws reader.
    NOTIFY(std::move(message), handle_complete, _1, error::success);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
