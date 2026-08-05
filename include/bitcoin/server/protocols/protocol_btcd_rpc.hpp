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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_RPC_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_RPC_HPP

#include <atomic>
#include <memory>
#include <set>
#include <unordered_set>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

namespace libbitcoin {
namespace server {

/// btcd-compatible json-rpc-v1 over http/ws. Inherits the bitcoind interface
/// handlers (getblockcount etc.) from protocol_bitcoind_rpc and adds the
/// btcd-only extension methods via a second dispatcher. Both dispatchers are
/// bridged in both directions, so all methods are reachable over both
/// transports. ws authorization is established once per connection, by basic
/// auth on the ws upgrade request or by the in-band 'authenticate' method,
/// and latched on the channel (see dispatch_websocket).
class BCS_API protocol_btcd_rpc
  : public server::protocol_bitcoind_rpc,
    protected network::tracker<protocol_btcd_rpc>
{
public:
    typedef std::shared_ptr<protocol_btcd_rpc> ptr;
    using btcd_interface = interface::btcd;
    using btcd_dispatcher = network::rpc::dispatcher<btcd_interface>;

    inline protocol_btcd_rpc(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_bitcoind_rpc(session, channel, options),
        network::tracker<protocol_btcd_rpc>(session->log),
        options_(options),
        p2kh_(session->system_settings().forks.difficult ?
            system::wallet::payment_address::mainnet_p2kh :
            system::wallet::payment_address::testnet_p2kh),
        p2sh_(session->system_settings().forks.difficult ?
            system::wallet::payment_address::mainnet_p2sh :
            system::wallet::payment_address::testnet_p2sh)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Dispatch btcd ws frames: tries the btcd-only dispatcher first, then
    /// falls back to the inherited bitcoind dispatcher (dispatch_rpc).
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Post-side mirror of the above: lets handle_receive_post reach the
    /// btcd-only methods over plain http post too (real clients issue e.g.
    /// getinfo as post-based capability checks before any ws traffic).
    code dispatch_extension(
        const network::rpc::request_t& message) NOEXCEPT override;

    /// Handlers (authentication/admin). A failed authenticate ends the
    /// session, once the error response has reached the client (btcd closes
    /// the connection on invalid credentials).
    bool handle_authenticate(const code& ec, btcd_interface::authenticate,
        const std::string& username, const std::string& password) NOEXCEPT;
    bool handle_session(const code& ec, btcd_interface::session) NOEXCEPT;

    /// Handler (network magic).
    bool handle_get_current_net(const code& ec,
        btcd_interface::get_current_net) NOEXCEPT;

    /// Handler (chain tip {hash, height}).
    bool handle_get_best_block(const code& ec,
        btcd_interface::get_best_block) NOEXCEPT;

    /// Handlers (generic btcd-tooling compatibility).
    bool handle_get_difficulty(const code& ec,
        btcd_interface::get_difficulty) NOEXCEPT;
    bool handle_get_info(const code& ec, btcd_interface::get_info) NOEXCEPT;
    bool handle_get_net_totals(const code& ec,
        btcd_interface::get_net_totals) NOEXCEPT;
    bool handle_get_network_hash_ps(const code& ec,
        btcd_interface::get_network_hash_ps, uint32_t blocks,
        int32_t height) NOEXCEPT;
    bool handle_create_raw_transaction(const code& ec,
        btcd_interface::create_raw_transaction,
        const network::rpc::array_t& inputs,
        const network::rpc::object_t& outputs, uint32_t locktime) NOEXCEPT;
    bool handle_decode_raw_transaction(const code& ec,
        btcd_interface::decode_raw_transaction,
        const std::string& hexstring) NOEXCEPT;
    bool handle_decode_script(const code& ec,
        btcd_interface::decode_script, const std::string& hex) NOEXCEPT;
    bool handle_validate_address(const code& ec,
        btcd_interface::validate_address,
        const std::string& address) NOEXCEPT;
    bool handle_help(const code& ec, btcd_interface::help,
        const std::string& command) NOEXCEPT;

    /// Address string (base58 or bech32/bech32m) to output script, for
    /// createrawtransaction.
    code parse_output_script(const std::string& text,
        system::chain::script& out) NOEXCEPT;

    /// Handlers (block subscription).
    bool handle_notify_blocks(const code& ec,
        btcd_interface::notify_blocks) NOEXCEPT;
    bool handle_stop_notify_blocks(const code& ec,
        btcd_interface::stop_notify_blocks) NOEXCEPT;

    /// Handlers (mempool subscription, not_implemented pending v5 mempool).
    bool handle_notify_new_transactions(const code& ec,
        btcd_interface::notify_new_transactions, bool verbose) NOEXCEPT;
    bool handle_stop_notify_new_transactions(const code& ec,
        btcd_interface::stop_notify_new_transactions) NOEXCEPT;

    /// Handlers (address/outpoint filtering). notifyblocks arms delivery;
    /// loadtxfilter only populates the filter (as btcd). rescanblocks
    /// replays the match logic against explicitly named historical blocks.
    bool handle_load_tx_filter(const code& ec,
        btcd_interface::load_tx_filter, bool reload,
        const network::rpc::value_t& addresses,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_rescan_blocks(const code& ec,
        btcd_interface::rescan_blocks,
        const network::rpc::value_t& blockhashes) NOEXCEPT;

    /// Filter parsing (loadtxfilter). Merges into the existing filter unless
    /// reload clears it first.
    code parse_filter_addresses(bool reload,
        const network::rpc::value_t& addresses) NOEXCEPT;
    code parse_filter_outpoints(bool reload,
        const network::rpc::value_t& outpoints) NOEXCEPT;

    /// Match a block's transactions against the loaded filter: an output
    /// paying a watched address, or an input spending a watched outpoint. A
    /// matched output's own outpoint is added to the watched set (as btcd).
    /// Returns the matched transactions as base16, in block order.
    network::rpc::array_t match_filtered_transactions(
        const system::chain::block& block) NOEXCEPT;

    /// Handler (admin, permanently not_implemented).
    bool handle_stop(const code& ec, btcd_interface::stop) NOEXCEPT;

    /// Handlers (deprecated, permanently not_implemented).
    bool handle_notify_received(const code& ec,
        btcd_interface::notify_received,
        const network::rpc::value_t& addresses) NOEXCEPT;
    bool handle_stop_notify_received(const code& ec,
        btcd_interface::stop_notify_received,
        const network::rpc::value_t& addresses) NOEXCEPT;
    bool handle_notify_spent(const code& ec,
        btcd_interface::notify_spent,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_stop_notify_spent(const code& ec,
        btcd_interface::stop_notify_spent,
        const network::rpc::value_t& outpoints) NOEXCEPT;
    bool handle_rescan(const code& ec, btcd_interface::rescan,
        const std::string& beginblock, const network::rpc::value_t& addresses,
        const network::rpc::value_t& outpoints,
        const std::string& endblock) NOEXCEPT;

    /// Chase event subscription (block connect/disconnect for notifyblocks).
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;
    void do_block_connected(node::header_t link) NOEXCEPT;
    void do_block_disconnected(node::header_t link) NOEXCEPT;

    /// Senders (btcd envelope, distinct id/version cache from the inherited
    /// senders). close_reason (if truthy) stops the channel only once the
    /// write has completed, so the error reaches the client first.
    void send_btcd_result(network::rpc::value_option&& result,
        size_t size_hint) NOEXCEPT;
    void send_btcd_error(const code& ec) NOEXCEPT;
    void send_btcd_error(const code& ec, size_t size_hint) NOEXCEPT;
    void send_btcd_error(const code& ec, size_t size_hint,
        const code& close_reason) NOEXCEPT;
    void send_btcd_notification(const std::string& method,
        network::rpc::array_t&& params, size_t size_hint) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void btcd_subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        btcd_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // Senders.
    void send_btcd_rpc(network::rpc::response_t&& model, size_t size_hint,
        const code& close_reason) NOEXCEPT;

    // These are thread safe.
    std::atomic_bool subscribed_blocks_{};
    const options_t& options_;
    const uint8_t p2kh_;
    const uint8_t p2sh_;

    // These are protected by strand.
    btcd_dispatcher btcd_dispatcher_{};
    network::rpc::version btcd_version_{};
    network::rpc::id_option btcd_id_{};

    // Tx filter (loadtxfilter). Hash sets are kept per script template so a
    // p2kh watch never matches a p2wpkh output or vice versa.
    std::unordered_set<system::short_hash> pay_key_hashes_{};
    std::unordered_set<system::short_hash> pay_script_hashes_{};
    std::set<system::data_chunk> pay_witness_key_hash_programs_{};
    std::set<system::data_chunk> pay_witness_script_hash_programs_{};
    std::set<system::data_chunk> pay_witness_taproot_programs_{};
    std::unordered_set<system::chain::point> filter_outpoints_{};
};

} // namespace server
} // namespace libbitcoin

#endif
