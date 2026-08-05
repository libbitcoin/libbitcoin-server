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
// ws frames carry no headers, so ws authorization is enforced here, not by
// the channel. It is established at most once per connection, by basic auth
// on the upgrade request or by exactly one authenticate call -- so a call is
// invalid if authorized and the method is authenticate, or not authorized
// and the method is not authenticate (as btcd).

void protocol_btcd_rpc::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // btcd websocket frames are json-rpc text (channel default reader).
    if (!request.body().contains<http::string_value>())
    {
        send_btcd_error(error::invalid_argument);
        return;
    }

    request_t message{};
    try
    {
        message = value_to<request_t>(parse(
            request.body().get<http::string_value>()));
    }
    catch (const std::exception&)
    {
        send_btcd_error(error::invalid_argument);
        return;
    }

    // Cache request context for response building (version + id).
    btcd_version_ = message.jsonrpc;
    btcd_id_ = message.id;

    const auto authenticate =
        (message.method == btcd_interface::authenticate::name);

    if (authenticate == authorized())
    {
        send_btcd_error(network::error::unauthorized);
        return;
    }

    if (!authenticate && !permitted(message.method))
    {
        send_btcd_error(network::error::unauthorized);
        return;
    }

    // Fall back to the inherited chain dispatcher on a method-name lookup
    // miss (notify never returns unexpected_method for an argument mismatch,
    // so the fallback cannot mask a real handler error).
    auto code = btcd_dispatcher_.notify(message);
    if (code == network::error::unexpected_method)
        code = dispatch_rpc(message);

    if (!code)
        return;

    // Unknown method: reply with an error and keep the connection alive.
    if (code == network::error::unexpected_method)
        send_btcd_error(code);
    else
        stop(code);
}

// Post-side mirror of the above: reached from the inherited
// handle_receive_post, so btcd-only methods are also callable over post.
code protocol_btcd_rpc::dispatch_extension(
    const network::rpc::request_t& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    btcd_version_ = message.jsonrpc;
    btcd_id_ = message.id;
    return btcd_dispatcher_.notify(message);
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
        send_btcd_result({}, 4);
        return true;
    }

    // A failed authenticate ends the session (as btcd), once the error has
    // reached the client.
    const code unauthorized{ network::error::unauthorized };
    send_btcd_error(unauthorized, two * unauthorized.message().size(),
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
    send_btcd_result(value_t{ std::move(result) }, 32);
    return true;
}

bool protocol_btcd_rpc::handle_get_current_net(const code& ec,
    btcd_interface::get_current_net) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The p2p handshake magic, the same value btcd returns (checked once by
    // btcwallet/lnd at connect to confirm the expected network).
    send_btcd_result(
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
    send_btcd_result(value_t{ std::move(result) }, 96);
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
        send_btcd_error(error::not_found, two * beginblock.size());
        return true;
    }

    const auto& query = archive();
    if (query.to_header(begin_hash).is_terminal())
    {
        send_btcd_error(error::not_found, two * beginblock.size());
        return true;
    }

    const auto has_addresses = std::holds_alternative<array_t>(
        addresses.value()) && !std::get<array_t>(addresses.value()).empty();
    const auto has_outpoints = std::holds_alternative<array_t>(
        outpoints.value()) && !std::get<array_t>(outpoints.value()).empty();

    if (has_addresses || has_outpoints)
    {
        send_btcd_error(error::not_implemented);
        return true;
    }

    // Nothing to watch: report the current chain tip as already-finished,
    // ignoring beginblock (as btcd).
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_btcd_error(error::not_found);
        return true;
    }

    // Reply to the rescan call itself before the unprompted notification.
    send_btcd_result({}, 4);

    array_t params{};
    params.emplace_back(value_t{ encode_hash(query.get_top_confirmed_hash()) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(top) });
    params.emplace_back(value_t{
        possible_sign_cast<int64_t>(header->timestamp()) });
    send_btcd_notification("rescanfinished", std::move(params), 256);
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

    // btcd 'blockconnected' notification: [hash, height, time].
    array_t params{};
    params.emplace_back(value_t{ encode_hash(query.get_header_key(link)) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(height) });
    params.emplace_back(value_t{ possible_sign_cast<int64_t>(
        header->timestamp()) });

    send_btcd_notification("blockconnected", std::move(params), 256);

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

    send_btcd_notification("filteredblockconnected",
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

    send_btcd_notification("blockdisconnected", std::move(params), 256);

    // btcd 'filteredblockdisconnected': [height, header].
    array_t filtered_params{};
    filtered_params.emplace_back(value_t{
        possible_sign_cast<int64_t>(height) });
    filtered_params.emplace_back(value_t{
        to_text(*header, chain::header::serialized_size()) });

    send_btcd_notification("filteredblockdisconnected",
        std::move(filtered_params), 256);
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
    }, size_hint, error::success);
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

    // NOTIFY does not restart the (already active) ws reader.
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

    // handle_complete stops (if close_reason is truthy) only once the write
    // has completed; SEND restarts the ws reader on success.
    SEND(std::move(message), handle_complete, _1, close_reason);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
