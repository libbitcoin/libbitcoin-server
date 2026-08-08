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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_rpc
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rpc::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_best_block_hash, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_chain_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_filter, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_hash, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_header, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_chain_tx_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_tx_out, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_tx_out_set_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prune_block_chain, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_save_mempool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_tx_out_set, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_chain, _1, _2, _3, _4);

    SUBSCRIBE_BITCOIND(handle_help, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_network_hash_ps, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_network_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_raw_transaction, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_BITCOIND(handle_decode_raw_transaction, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_raw_transaction, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_send_raw_transaction, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_test_mempool_accept, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_decode_script, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_validate_address, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_dump_tx_out_set, _1, _2);
    SUBSCRIBE_BITCOIND(handle_load_tx_out_set, _1, _2);
    SUBSCRIBE_BITCOIND(handle_clear_banned, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_banned, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_ban, _1, _2);
    SUBSCRIBE_BITCOIND(handle_stop, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_out_proof, _1, _2);
    SUBSCRIBE_BITCOIND(handle_verify_tx_out_proof, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_from_peer, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_chain_states, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_chain_tips, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_deployment_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_descriptor_activity, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_difficulty, _1, _2);
    SUBSCRIBE_BITCOIND(handle_precious_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_blocks, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wait_for_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wait_for_block_height, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wait_for_new_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_analyze_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_combine_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_convert_to_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_decode_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_finalize_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_join_psbts, _1, _2);
    SUBSCRIBE_BITCOIND(handle_descriptor_process_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_utxo_update_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mining_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_header, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_node, _1, _2);
    SUBSCRIBE_BITCOIND(handle_disconnect_node, _1, _2);
    SUBSCRIBE_BITCOIND(handle_export_asmap, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_added_node_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_addrman_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_connection_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_net_totals, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_node_addresses, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_peer_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_ping, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_network_active, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_multisig, _1, _2);
    SUBSCRIBE_BITCOIND(handle_derive_addresses, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_descriptor_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_verify_message, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_index_info, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_memory_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_openrpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_rpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_logging, _1, _2);
    SUBSCRIBE_BITCOIND(handle_uptime, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_zmq_notifications, _1, _2);
    SUBSCRIBE_BITCOIND(handle_enumerate_signers, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_connection, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_peer_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_echo, _1, _2);
    SUBSCRIBE_BITCOIND(handle_echo_ipc, _1, _2);
    SUBSCRIBE_BITCOIND(handle_echo_json, _1, _2);
    SUBSCRIBE_BITCOIND(handle_estimate_raw_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate_to_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate_to_descriptor, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_fee_rate_diagram, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_orphan_txs, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_addrman, _1, _2);
    SUBSCRIBE_BITCOIND(handle_invalidate_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_mock_scheduler, _1, _2);
    SUBSCRIBE_BITCOIND(handle_reconsider_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_msg_to_peer, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_mock_time, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sync_with_validation_interface_queue, _1, _2);
    SUBSCRIBE_BITCOIND(handle_abandon_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_abort_rescan, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_hd_key, _1, _2);
    SUBSCRIBE_BITCOIND(handle_backup_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_bump_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_wallet_descriptor, _1, _2);
    SUBSCRIBE_BITCOIND(handle_encrypt_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_export_watchonly_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_addresses_by_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_address_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_balance, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_balances, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_hd_keys, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_new_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_change_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_received_by_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_received_by_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_wallet_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_descriptors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_pruned_funds, _1, _2);
    SUBSCRIBE_BITCOIND(handle_keypool_refill, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_address_groupings, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_descriptors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_labels, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_lock_unspent, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_received_by_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_received_by_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_since_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_transactions, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_unspent, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_wallet_dir, _1, _2);
    SUBSCRIBE_BITCOIND(handle_list_wallets, _1, _2);
    SUBSCRIBE_BITCOIND(handle_load_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_lock_unspent, _1, _2);
    SUBSCRIBE_BITCOIND(handle_migrate_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_psbt_bump_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_remove_pruned_funds, _1, _2);
    SUBSCRIBE_BITCOIND(handle_rescan_block_chain, _1, _2);
    SUBSCRIBE_BITCOIND(handle_restore_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_all, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_many, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_to_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_label, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_wallet_flag, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sign_message, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sign_raw_transaction_with_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_simulate_raw_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_unload_wallet, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_create_funded_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_display_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_lock, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_passphrase, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_passphrase_change, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wallet_process_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_ancestors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_cluster, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_descendants, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_entry, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_mempool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_spending_prevout, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_mempool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_abort_private_broadcast, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_private_broadcast_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_package, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_template, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_prioritised_transactions, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prioritise_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_estimate_smart_fee, _1, _2);
    network::protocol_http::start();
}

void protocol_bitcoind_rpc::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rpc_dispatcher_.stop(ec);
    network::protocol_http::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

// Handled here for rpc and derived rest protocol.
void protocol_bitcoind_rpc::handle_receive_options(const code& ec,
    const options::cptr& options) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*options, options->version()))
    {
        send_bad_host(*options);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*options, options->version()))
    {
        send_forbidden(*options);
        return;
    }

    send_ok(*options);
}

// Derived rest protocol handles get and rpc handles post.
void protocol_bitcoind_rpc::handle_receive_post(const code& ec,
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

    // Get the parsed json-rpc request object.
    // v1 or v2 both supported, batch not yet supported.
    // v1 null id and v2 missing id implies notification and no response.
    const auto& message = post->body().get<request>().message;

    // The post is saved off during asynchonous handling and used in send_json
    // to formulate response headers, isolating handlers from http semantics.
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_error(error::unauthorized);
        return;
    }

    // Dispatch the request to the interface dispatcher.
    const auto code = rpc_dispatcher_.notify(message);
    if (!code)
        return;

    // Unknown method: reply with an error and keep the connection alive.
    if (code == network::error::unexpected_method)
        send_error(code);
    else
        stop(code);
}

// The websocket transport of the interface: parse the ws text frame as a
// json-rpc request and dispatch as an http post would.
void protocol_bitcoind_rpc::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // ws frames carry no headers, so ws authorization is enforced here, not
    // by the channel (established by basic auth on the upgrade request).
    if (!authorized())
    {
        stop(network::error::unauthorized);
        return;
    }

    // Websocket frames are parsed as json-rpc requests (channel body type).
    if (!request.body().contains<rpc::request>())
    {
        stop(error::invalid_argument);
        return;
    }

    const auto& message = request.body().get<rpc::request>().message;

    // Cache request context for response building (version + id).
    set_rpc_request(message);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_error(error::unauthorized);
        return;
    }

    // Dispatch the request to the interface dispatcher.
    const auto code = rpc_dispatcher_.notify(message);
    if (!code)
        return;

    // Unknown method: reply with an error and keep the connection alive.
    if (code == network::error::unexpected_method)
        send_error(code);
    else
        stop(code);
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rpc::send_error(const code& ec) NOEXCEPT
{
    send_error(ec, two * ec.message().size());
}

void protocol_bitcoind_rpc::send_error(const code& ec,
    size_t size_hint) NOEXCEPT
{
    send_error(ec, {}, size_hint);
}

void protocol_bitcoind_rpc::send_error(const code& ec, size_t size_hint,
    const code& close_reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .error = result_t
        {
            .code = ec.value(),
            .message = ec.message()
        }
    }, size_hint, close_reason);
}

void protocol_bitcoind_rpc::send_error(const code& ec,
    value_option&& error, size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .error = result_t
        {
            .code = ec.value(),
            .message = ec.message(),
            .data = std::move(error)
        }
    }, size_hint);
}

void protocol_bitcoind_rpc::send_text(std::string&& hexidecimal) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_result(hexidecimal, hexidecimal.size());
}

void protocol_bitcoind_rpc::send_result(value_option&& result,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .result = std::move(result)
    }, size_hint);
}

// private
void protocol_bitcoind_rpc::send_rpc(response_t&& model,
    size_t size_hint) NOEXCEPT
{
    send_rpc(std::move(model), size_hint, error::success);
}

void protocol_bitcoind_rpc::send_rpc(response_t&& model, size_t size_hint,
    const code& close_reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace http;
    static const auto json = from_media_type(media_type::application_json);

    if (websocket())
    {
        id_.reset();
        version_ = version::undefined;
        http::response message{ status::ok, 11 };
        message.set(field::content_type, json);
        message.body() = rpc::response
        {
            { .size_hint = size_hint }, std::move(model),
        };
        message.prepare_payload();
        SEND(std::move(message), handle_complete, _1, close_reason);
        return;
    }

    const auto request = reset_rpc_request();
    http::response message{ status::ok, request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(field::content_type, json);
    message.body() = rpc::response
    {
        { .size_hint = size_hint }, std::move(model),
    };
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, close_reason);
}

// protected
void protocol_bitcoind_rpc::set_rpc_request(version version,
    const id_option& id, const http::request_cptr& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    id_ = id;
    version_ = version;
    set_request(request);
}

// protected
void protocol_bitcoind_rpc::set_rpc_request(const request_t& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    id_ = message.id;
    version_ = message.jsonrpc;
}

// private
http::request_cptr protocol_bitcoind_rpc::reset_rpc_request() NOEXCEPT
{
    BC_ASSERT(stranded());
    id_.reset();
    version_ = version::undefined;
    return reset_request();
}

// utility (redundant with protocol_electrum)
// ----------------------------------------------------------------------------
// TODO: move this to node utility and pass through.

bool protocol_bitcoind_rpc::get_pool_context(chain::context& pool) const NOEXCEPT
{
    const auto& query = archive();
    const auto& settings = system_settings();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);
    const auto hash = query.get_header_key(link);
    const auto state = query.get_chain_state(settings, hash);
    if (!state) return false;
    pool = chain::chain_state(*state, settings).context();
    return true;
}

code protocol_bitcoind_rpc::validate_tx(
    const chain::transaction& tx) const NOEXCEPT
{
    chain::context ctx{};
    if (!get_pool_context(ctx))
        return error::server_error;

    code ec{};

    // Ensure tx does not violate tx consensus rules.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(ctx);
    if (!ec) archive().populate_with_metadata(tx, true);
    if (!ec) ec = tx.accept(ctx);
    if (!ec) ec = tx.confirm(ctx);
    if (!ec) ec = tx.connect(ctx);

    // Ensure tx does not violate presumed block consensus rules.
    // This is a DoS guard when validating a tx outside of a block.
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(ctx);
    if (!ec) ec = tx.accept_guard(ctx);
    return ec;
}

code protocol_bitcoind_rpc::broadcast_tx(
    const chain::transaction::cptr& tx) NOEXCEPT
{
    if (const auto ec = validate_tx(*tx))
        return ec;

    BROADCAST(peer::transaction, to_shared<peer::transaction>(tx));
    return {};
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
