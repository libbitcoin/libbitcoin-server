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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_RPC_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_RPC_HPP

#include <memory>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_bitcoind_rpc
  : public server::protocol_http,
    protected network::tracker<protocol_bitcoind_rpc>
{
public:
    // Replace base class channel_t (json-rpc websocket reader body).
    using channel_t = channel_http<network::rpc::request>;

    typedef std::shared_ptr<protocol_bitcoind_rpc> ptr;
    using rpc_interface = interface::bitcoind_rpc;
    using rpc_dispatcher = network::rpc::dispatcher<rpc_interface>;

    inline protocol_bitcoind_rpc(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_http(session, channel, options),
        network::tracker<protocol_bitcoind_rpc>(session->log),
        p2kh_(session->server_settings().wallet.p2kh_prefix),
        p2sh_(session->server_settings().wallet.p2sh_prefix),
        witness_(session->server_settings().wallet.witness_prefix)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    using post = network::http::method::post;
    using options = network::http::method::options;

    /// Dispatch (the post and websocket transports of the interface).
    void handle_receive_options(const code& ec,
        const network::http::method::options::cptr& options) NOEXCEPT override;
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Handlers.
    bool handle_get_best_block_hash(const code& ec,
        rpc_interface::get_best_block_hash) NOEXCEPT;
    bool handle_get_block(const code& ec,
        rpc_interface::get_block, const std::string&,
        double verbosity) NOEXCEPT;
    bool handle_get_block_chain_info(const code& ec,
        rpc_interface::get_block_chain_info) NOEXCEPT;
    bool handle_get_block_count(const code& ec,
        rpc_interface::get_block_count) NOEXCEPT;
    bool handle_get_block_filter(const code& ec,
        rpc_interface::get_block_filter, const std::string&,
        const std::string&) NOEXCEPT;
    bool handle_get_block_hash(const code& ec,
        rpc_interface::get_block_hash, double) NOEXCEPT;
    bool handle_get_block_header(const code& ec,
        rpc_interface::get_block_header, const std::string&, bool) NOEXCEPT;
    bool handle_get_block_stats(const code& ec,
        rpc_interface::get_block_stats, const network::rpc::value_t&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_get_chain_tx_stats(const code& ec,
        rpc_interface::get_chain_tx_stats, double,
        const std::string&) NOEXCEPT;
    bool handle_get_tx_out(const code& ec,
        rpc_interface::get_tx_out, const std::string&, double, bool) NOEXCEPT;
    bool handle_get_tx_out_set_info(const code& ec,
        rpc_interface::get_tx_out_set_info) NOEXCEPT;
    bool handle_prune_block_chain(const code& ec,
        rpc_interface::prune_block_chain, double) NOEXCEPT;
    bool handle_save_mempool(const code& ec,
        rpc_interface::save_mempool) NOEXCEPT;
    bool handle_scan_tx_out_set(const code& ec,
        rpc_interface::scan_tx_out_set, const std::string&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_verify_chain(const code& ec,
        rpc_interface::verify_chain, double, double) NOEXCEPT;
    bool handle_help(const code& ec, rpc_interface::help,
        const std::string& command) NOEXCEPT;
    bool handle_get_network_hash_ps(const code& ec,
        rpc_interface::get_network_hash_ps, uint32_t nblocks,
        int32_t height) NOEXCEPT;
    bool handle_get_network_info(const code& ec,
        rpc_interface::get_network_info) NOEXCEPT;
    bool handle_get_raw_transaction(const code& ec,
        rpc_interface::get_raw_transaction, const std::string& txid,
        double verbose, const std::string& blockhash) NOEXCEPT;
    bool handle_send_raw_transaction(const code& ec,
        rpc_interface::send_raw_transaction, const std::string& hexstring,
        double maxfeerate) NOEXCEPT;
    bool handle_create_raw_transaction(const code& ec,
        rpc_interface::create_raw_transaction,
        const network::rpc::array_t& inputs,
        const network::rpc::object_t& outputs, uint32_t locktime,
        bool replaceable) NOEXCEPT;
    bool handle_decode_raw_transaction(const code& ec,
        rpc_interface::decode_raw_transaction,
        const std::string& hexstring) NOEXCEPT;
    bool handle_test_mempool_accept(const code& ec,
        rpc_interface::test_mempool_accept,
        const network::rpc::array_t& rawtxs, uint32_t maxfeerate) NOEXCEPT;
    bool handle_decode_script(const code& ec,
        rpc_interface::decode_script, const std::string& hex) NOEXCEPT;
    bool handle_validate_address(const code& ec,
        rpc_interface::validate_address,
        const std::string& address) NOEXCEPT;

    bool handle_dump_tx_out_set(const code& ec,
        rpc_interface::dump_tx_out_set) NOEXCEPT;
    bool handle_load_tx_out_set(const code& ec,
        rpc_interface::load_tx_out_set) NOEXCEPT;
    bool handle_clear_banned(const code& ec,
        rpc_interface::clear_banned) NOEXCEPT;
    bool handle_list_banned(const code& ec,
        rpc_interface::list_banned) NOEXCEPT;
    bool handle_set_ban(const code& ec,
        rpc_interface::set_ban) NOEXCEPT;
    bool handle_stop(const code& ec,
        rpc_interface::stop) NOEXCEPT;
    bool handle_get_tx_out_proof(const code& ec,
        rpc_interface::get_tx_out_proof) NOEXCEPT;
    bool handle_verify_tx_out_proof(const code& ec,
        rpc_interface::verify_tx_out_proof) NOEXCEPT;
    bool handle_get_block_from_peer(const code& ec,
        rpc_interface::get_block_from_peer) NOEXCEPT;
    bool handle_get_chain_states(const code& ec,
        rpc_interface::get_chain_states) NOEXCEPT;
    bool handle_get_chain_tips(const code& ec,
        rpc_interface::get_chain_tips) NOEXCEPT;
    bool handle_get_deployment_info(const code& ec,
        rpc_interface::get_deployment_info) NOEXCEPT;
    bool handle_get_descriptor_activity(const code& ec,
        rpc_interface::get_descriptor_activity) NOEXCEPT;
    bool handle_get_difficulty(const code& ec,
        rpc_interface::get_difficulty) NOEXCEPT;
    bool handle_precious_block(const code& ec,
        rpc_interface::precious_block) NOEXCEPT;
    bool handle_scan_blocks(const code& ec,
        rpc_interface::scan_blocks) NOEXCEPT;
    bool handle_wait_for_block(const code& ec,
        rpc_interface::wait_for_block) NOEXCEPT;
    bool handle_wait_for_block_height(const code& ec,
        rpc_interface::wait_for_block_height) NOEXCEPT;
    bool handle_wait_for_new_block(const code& ec,
        rpc_interface::wait_for_new_block) NOEXCEPT;
    bool handle_analyze_psbt(const code& ec,
        rpc_interface::analyze_psbt) NOEXCEPT;
    bool handle_combine_psbt(const code& ec,
        rpc_interface::combine_psbt) NOEXCEPT;
    bool handle_convert_to_psbt(const code& ec,
        rpc_interface::convert_to_psbt) NOEXCEPT;
    bool handle_create_psbt(const code& ec,
        rpc_interface::create_psbt) NOEXCEPT;
    bool handle_decode_psbt(const code& ec,
        rpc_interface::decode_psbt) NOEXCEPT;
    bool handle_finalize_psbt(const code& ec,
        rpc_interface::finalize_psbt) NOEXCEPT;
    bool handle_join_psbts(const code& ec,
        rpc_interface::join_psbts) NOEXCEPT;
    bool handle_descriptor_process_psbt(const code& ec,
        rpc_interface::descriptor_process_psbt) NOEXCEPT;
    bool handle_utxo_update_psbt(const code& ec,
        rpc_interface::utxo_update_psbt) NOEXCEPT;
    bool handle_get_mining_info(const code& ec,
        rpc_interface::get_mining_info) NOEXCEPT;
    bool handle_submit_block(const code& ec,
        rpc_interface::submit_block) NOEXCEPT;
    bool handle_submit_header(const code& ec,
        rpc_interface::submit_header) NOEXCEPT;
    bool handle_add_node(const code& ec,
        rpc_interface::add_node) NOEXCEPT;
    bool handle_disconnect_node(const code& ec,
        rpc_interface::disconnect_node) NOEXCEPT;
    bool handle_export_asmap(const code& ec,
        rpc_interface::export_asmap) NOEXCEPT;
    bool handle_get_added_node_info(const code& ec,
        rpc_interface::get_added_node_info) NOEXCEPT;
    bool handle_get_addrman_info(const code& ec,
        rpc_interface::get_addrman_info) NOEXCEPT;
    bool handle_get_connection_count(const code& ec,
        rpc_interface::get_connection_count) NOEXCEPT;
    bool handle_get_net_totals(const code& ec,
        rpc_interface::get_net_totals) NOEXCEPT;
    bool handle_get_node_addresses(const code& ec,
        rpc_interface::get_node_addresses) NOEXCEPT;
    bool handle_get_peer_info(const code& ec,
        rpc_interface::get_peer_info) NOEXCEPT;
    bool handle_ping(const code& ec,
        rpc_interface::ping) NOEXCEPT;
    bool handle_set_network_active(const code& ec,
        rpc_interface::set_network_active) NOEXCEPT;
    bool handle_create_multisig(const code& ec,
        rpc_interface::create_multisig) NOEXCEPT;
    bool handle_derive_addresses(const code& ec,
        rpc_interface::derive_addresses) NOEXCEPT;
    bool handle_get_descriptor_info(const code& ec,
        rpc_interface::get_descriptor_info) NOEXCEPT;
    bool handle_verify_message(const code& ec,
        rpc_interface::verify_message, const std::string& address,
        const std::string& signature, const std::string& message) NOEXCEPT;
    bool handle_get_index_info(const code& ec,
        rpc_interface::get_index_info, const std::string& index_name) NOEXCEPT;
    bool handle_get_memory_info(const code& ec,
        rpc_interface::get_memory_info) NOEXCEPT;
    bool handle_get_openrpc_info(const code& ec,
        rpc_interface::get_openrpc_info) NOEXCEPT;
    bool handle_get_rpc_info(const code& ec,
        rpc_interface::get_rpc_info) NOEXCEPT;
    bool handle_logging(const code& ec,
        rpc_interface::logging) NOEXCEPT;
    bool handle_uptime(const code& ec,
        rpc_interface::uptime) NOEXCEPT;
    bool handle_get_zmq_notifications(const code& ec,
        rpc_interface::get_zmq_notifications) NOEXCEPT;
    bool handle_enumerate_signers(const code& ec,
        rpc_interface::enumerate_signers) NOEXCEPT;
    bool handle_add_connection(const code& ec,
        rpc_interface::add_connection) NOEXCEPT;
    bool handle_add_peer_address(const code& ec,
        rpc_interface::add_peer_address) NOEXCEPT;
    bool handle_echo(const code& ec,
        rpc_interface::echo) NOEXCEPT;
    bool handle_echo_ipc(const code& ec,
        rpc_interface::echo_ipc) NOEXCEPT;
    bool handle_echo_json(const code& ec,
        rpc_interface::echo_json) NOEXCEPT;
    bool handle_estimate_raw_fee(const code& ec,
        rpc_interface::estimate_raw_fee) NOEXCEPT;
    bool handle_generate(const code& ec,
        rpc_interface::generate) NOEXCEPT;
    bool handle_generate_block(const code& ec,
        rpc_interface::generate_block) NOEXCEPT;
    bool handle_generate_to_address(const code& ec,
        rpc_interface::generate_to_address) NOEXCEPT;
    bool handle_generate_to_descriptor(const code& ec,
        rpc_interface::generate_to_descriptor) NOEXCEPT;
    bool handle_get_mempool_fee_rate_diagram(const code& ec,
        rpc_interface::get_mempool_fee_rate_diagram) NOEXCEPT;
    bool handle_get_orphan_txs(const code& ec,
        rpc_interface::get_orphan_txs) NOEXCEPT;
    bool handle_get_raw_addrman(const code& ec,
        rpc_interface::get_raw_addrman) NOEXCEPT;
    bool handle_invalidate_block(const code& ec,
        rpc_interface::invalidate_block) NOEXCEPT;
    bool handle_mock_scheduler(const code& ec,
        rpc_interface::mock_scheduler) NOEXCEPT;
    bool handle_reconsider_block(const code& ec,
        rpc_interface::reconsider_block) NOEXCEPT;
    bool handle_send_msg_to_peer(const code& ec,
        rpc_interface::send_msg_to_peer) NOEXCEPT;
    bool handle_set_mock_time(const code& ec,
        rpc_interface::set_mock_time) NOEXCEPT;
    bool handle_sync_with_validation_interface_queue(const code& ec,
        rpc_interface::sync_with_validation_interface_queue) NOEXCEPT;
    bool handle_abandon_transaction(const code& ec,
        rpc_interface::abandon_transaction) NOEXCEPT;
    bool handle_abort_rescan(const code& ec,
        rpc_interface::abort_rescan) NOEXCEPT;
    bool handle_add_hd_key(const code& ec,
        rpc_interface::add_hd_key) NOEXCEPT;
    bool handle_backup_wallet(const code& ec,
        rpc_interface::backup_wallet) NOEXCEPT;
    bool handle_bump_fee(const code& ec,
        rpc_interface::bump_fee) NOEXCEPT;
    bool handle_create_wallet(const code& ec,
        rpc_interface::create_wallet) NOEXCEPT;
    bool handle_create_wallet_descriptor(const code& ec,
        rpc_interface::create_wallet_descriptor) NOEXCEPT;
    bool handle_encrypt_wallet(const code& ec,
        rpc_interface::encrypt_wallet) NOEXCEPT;
    bool handle_export_watchonly_wallet(const code& ec,
        rpc_interface::export_watchonly_wallet) NOEXCEPT;
    bool handle_get_addresses_by_label(const code& ec,
        rpc_interface::get_addresses_by_label) NOEXCEPT;
    bool handle_get_address_info(const code& ec,
        rpc_interface::get_address_info) NOEXCEPT;
    bool handle_get_balance(const code& ec,
        rpc_interface::get_balance) NOEXCEPT;
    bool handle_get_balances(const code& ec,
        rpc_interface::get_balances) NOEXCEPT;
    bool handle_get_hd_keys(const code& ec,
        rpc_interface::get_hd_keys) NOEXCEPT;
    bool handle_get_new_address(const code& ec,
        rpc_interface::get_new_address) NOEXCEPT;
    bool handle_get_raw_change_address(const code& ec,
        rpc_interface::get_raw_change_address) NOEXCEPT;
    bool handle_get_received_by_address(const code& ec,
        rpc_interface::get_received_by_address) NOEXCEPT;
    bool handle_get_received_by_label(const code& ec,
        rpc_interface::get_received_by_label) NOEXCEPT;
    bool handle_get_transaction(const code& ec,
        rpc_interface::get_transaction) NOEXCEPT;
    bool handle_get_wallet_info(const code& ec,
        rpc_interface::get_wallet_info) NOEXCEPT;
    bool handle_import_descriptors(const code& ec,
        rpc_interface::import_descriptors) NOEXCEPT;
    bool handle_import_pruned_funds(const code& ec,
        rpc_interface::import_pruned_funds) NOEXCEPT;
    bool handle_keypool_refill(const code& ec,
        rpc_interface::keypool_refill) NOEXCEPT;
    bool handle_list_address_groupings(const code& ec,
        rpc_interface::list_address_groupings) NOEXCEPT;
    bool handle_list_descriptors(const code& ec,
        rpc_interface::list_descriptors) NOEXCEPT;
    bool handle_list_labels(const code& ec,
        rpc_interface::list_labels) NOEXCEPT;
    bool handle_list_lock_unspent(const code& ec,
        rpc_interface::list_lock_unspent) NOEXCEPT;
    bool handle_list_received_by_address(const code& ec,
        rpc_interface::list_received_by_address) NOEXCEPT;
    bool handle_list_received_by_label(const code& ec,
        rpc_interface::list_received_by_label) NOEXCEPT;
    bool handle_list_since_block(const code& ec,
        rpc_interface::list_since_block) NOEXCEPT;
    bool handle_list_transactions(const code& ec,
        rpc_interface::list_transactions) NOEXCEPT;
    bool handle_list_unspent(const code& ec,
        rpc_interface::list_unspent) NOEXCEPT;
    bool handle_list_wallet_dir(const code& ec,
        rpc_interface::list_wallet_dir) NOEXCEPT;
    bool handle_list_wallets(const code& ec,
        rpc_interface::list_wallets) NOEXCEPT;
    bool handle_load_wallet(const code& ec,
        rpc_interface::load_wallet) NOEXCEPT;
    bool handle_lock_unspent(const code& ec,
        rpc_interface::lock_unspent) NOEXCEPT;
    bool handle_migrate_wallet(const code& ec,
        rpc_interface::migrate_wallet) NOEXCEPT;
    bool handle_psbt_bump_fee(const code& ec,
        rpc_interface::psbt_bump_fee) NOEXCEPT;
    bool handle_remove_pruned_funds(const code& ec,
        rpc_interface::remove_pruned_funds) NOEXCEPT;
    bool handle_rescan_block_chain(const code& ec,
        rpc_interface::rescan_block_chain) NOEXCEPT;
    bool handle_restore_wallet(const code& ec,
        rpc_interface::restore_wallet) NOEXCEPT;
    bool handle_wallet_send(const code& ec,
        rpc_interface::send) NOEXCEPT;
    bool handle_send_all(const code& ec,
        rpc_interface::send_all) NOEXCEPT;
    bool handle_send_many(const code& ec,
        rpc_interface::send_many) NOEXCEPT;
    bool handle_send_to_address(const code& ec,
        rpc_interface::send_to_address) NOEXCEPT;
    bool handle_set_label(const code& ec,
        rpc_interface::set_label) NOEXCEPT;
    bool handle_set_wallet_flag(const code& ec,
        rpc_interface::set_wallet_flag) NOEXCEPT;
    bool handle_sign_message(const code& ec,
        rpc_interface::sign_message) NOEXCEPT;
    bool handle_sign_raw_transaction_with_wallet(const code& ec,
        rpc_interface::sign_raw_transaction_with_wallet) NOEXCEPT;
    bool handle_simulate_raw_transaction(const code& ec,
        rpc_interface::simulate_raw_transaction) NOEXCEPT;
    bool handle_unload_wallet(const code& ec,
        rpc_interface::unload_wallet) NOEXCEPT;
    bool handle_wallet_create_funded_psbt(const code& ec,
        rpc_interface::wallet_create_funded_psbt) NOEXCEPT;
    bool handle_wallet_display_address(const code& ec,
        rpc_interface::wallet_display_address) NOEXCEPT;
    bool handle_wallet_lock(const code& ec,
        rpc_interface::wallet_lock) NOEXCEPT;
    bool handle_wallet_passphrase(const code& ec,
        rpc_interface::wallet_passphrase) NOEXCEPT;
    bool handle_wallet_passphrase_change(const code& ec,
        rpc_interface::wallet_passphrase_change) NOEXCEPT;
    bool handle_wallet_process_psbt(const code& ec,
        rpc_interface::wallet_process_psbt) NOEXCEPT;
    bool handle_get_mempool_ancestors(const code& ec,
        rpc_interface::get_mempool_ancestors) NOEXCEPT;
    bool handle_get_mempool_cluster(const code& ec,
        rpc_interface::get_mempool_cluster) NOEXCEPT;
    bool handle_get_mempool_descendants(const code& ec,
        rpc_interface::get_mempool_descendants) NOEXCEPT;
    bool handle_get_mempool_entry(const code& ec,
        rpc_interface::get_mempool_entry) NOEXCEPT;
    bool handle_get_mempool_info(const code& ec,
        rpc_interface::get_mempool_info) NOEXCEPT;
    bool handle_get_raw_mempool(const code& ec,
        rpc_interface::get_raw_mempool) NOEXCEPT;
    bool handle_get_tx_spending_prevout(const code& ec,
        rpc_interface::get_tx_spending_prevout) NOEXCEPT;
    bool handle_import_mempool(const code& ec,
        rpc_interface::import_mempool) NOEXCEPT;
    bool handle_abort_private_broadcast(const code& ec,
        rpc_interface::abort_private_broadcast) NOEXCEPT;
    bool handle_get_private_broadcast_info(const code& ec,
        rpc_interface::get_private_broadcast_info) NOEXCEPT;
    bool handle_submit_package(const code& ec,
        rpc_interface::submit_package) NOEXCEPT;
    bool handle_get_block_template(const code& ec,
        rpc_interface::get_block_template) NOEXCEPT;
    bool handle_get_prioritised_transactions(const code& ec,
        rpc_interface::get_prioritised_transactions) NOEXCEPT;
    bool handle_prioritise_transaction(const code& ec,
        rpc_interface::prioritise_transaction) NOEXCEPT;
    bool handle_estimate_smart_fee(const code& ec,
        rpc_interface::estimate_smart_fee) NOEXCEPT;
    /// The method names reported by help (btcd prepends its own).
    virtual std::string help_names() const NOEXCEPT;

    /// Serialize an object (chain::header, chain::transaction, ...) to a
    /// base16 string.
    template <typename Object, typename ...Args>
    static std::string to_text(const Object& object, size_t size,
        Args&&... args) NOEXCEPT
    {
        std::string out(two * size, '\0');
        system::stream::out::fast sink{ out };
        system::write::base16::fast writer{ sink };
        object.to_data(writer, std::forward<Args>(args)...);
        BC_ASSERT(writer);
        return out;
    }

    /// Json context helpers (shared with rest, defined in *_json.cpp).
    static uint32_t median_time_past(const node::query& query,
        const database::header_link& link) NOEXCEPT;
    static void inject_block_context(boost::json::object& out,
        const node::query& query, const database::header_link& link,
        const system::chain::header& header) NOEXCEPT;
    static void inject_tx_context(boost::json::object& out,
        const node::query& query, const database::tx_link& link) NOEXCEPT;
    static boost::json::object header_to_bitcoind(
        const system::chain::header& header) NOEXCEPT;
    static std::string chain_name(const node::query& query) NOEXCEPT;

    /// Senders. close_reason (if truthy) stops the channel only once the
    /// write has completed, so the error reaches the client first.
    void send_error(const code& ec) NOEXCEPT;
    void send_error(const code& ec, size_t size_hint) NOEXCEPT;
    void send_error(const code& ec, size_t size_hint,
        const code& close_reason) NOEXCEPT;
    void send_error(const code& ec, network::rpc::value_option&& error,
        size_t size_hint) NOEXCEPT;
    void send_text(std::string&& hexidecimal) NOEXCEPT;
    void send_result(network::rpc::value_option&& result,
        size_t size_hint) NOEXCEPT;

    /// Cache rpc response context for serialization (requires strand). The
    /// websocket overload has no http request to echo headers from.
    void set_rpc_request(network::rpc::version version,
        const network::rpc::id_option& id,
        const network::http::request_cptr& request) NOEXCEPT;
    void set_rpc_request(const network::rpc::request_t& message) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        rpc_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // Senders.
    void send_rpc(network::rpc::response_t&& model,
        size_t size_hint) NOEXCEPT;
    void send_rpc(network::rpc::response_t&& model, size_t size_hint,
        const code& close_reason) NOEXCEPT;

    // Validate a transaction given next block context.
    bool get_pool_context(system::chain::context& pool) const NOEXCEPT;
    code validate_tx(const system::chain::transaction& tx) const NOEXCEPT;
    code broadcast_tx(const system::chain::transaction::cptr& tx) NOEXCEPT;

    // Obtain cached request and clear cache (requires strand).
    network::http::request_cptr reset_rpc_request() NOEXCEPT;

    // These are protected by strand.
    rpc_dispatcher rpc_dispatcher_{};
    network::rpc::version version_{};
    network::rpc::id_option id_{};

protected:
    // These are thread safe.
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    const std::string witness_;
};

} // namespace server
} // namespace libbitcoin

#endif
