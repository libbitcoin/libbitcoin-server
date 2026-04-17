/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ELECTRUM_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ELECTRUM_HPP

#include <memory>
#include <unordered_set>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/protocols/protocol_electrum_version.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_electrum
  : public protocol_rpc<channel_electrum>,
    protected network::tracker<protocol_electrum>
{
public:
    typedef std::shared_ptr<protocol_electrum> ptr;
    using rpc_interface = interface::electrum;

    inline protocol_electrum(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<channel_electrum>(session, channel, options),
        options_(options),
        turbo_(session->database_settings().turbo),
        p2kh_(session->system_settings().forks.difficult ?
            system::wallet::payment_address::mainnet_p2kh :
            system::wallet::payment_address::testnet_p2kh),
        p2sh_(session->system_settings().forks.difficult ?
            system::wallet::payment_address::mainnet_p2sh :
            system::wallet::payment_address::testnet_p2sh),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        network::tracker<protocol_electrum>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Handlers (event subscription).
    bool handle_event(const code&, node::chase event_,
        node::event_value) NOEXCEPT;

    /// Handlers (headers).
    void handle_blockchain_number_of_blocks_subscribe(const code& ec,
        rpc_interface::blockchain_number_of_blocks_subscribe) NOEXCEPT;
    void handle_blockchain_block_get_chunk(const code& ec,
        rpc_interface::blockchain_block_get_chunk, double index) NOEXCEPT;
    void handle_blockchain_block_get_header(const code& ec,
        rpc_interface::blockchain_block_get_header, double height) NOEXCEPT;
    void handle_blockchain_block_header(const code& ec,
        rpc_interface::blockchain_block_header, double height,
        double cp_height) NOEXCEPT;
    void handle_blockchain_block_headers(const code& ec,
        rpc_interface::blockchain_block_headers, double start_height,
        double count, double cp_height) NOEXCEPT;
    void handle_blockchain_headers_subscribe(const code& ec,
        rpc_interface::blockchain_headers_subscribe, bool raw) NOEXCEPT;

    /// Handlers (fees).
    void handle_blockchain_estimate_fee(const code& ec,
        rpc_interface::blockchain_estimate_fee, double number,
        const std::string& mode) NOEXCEPT;
    void handle_blockchain_relay_fee(const code& ec,
        rpc_interface::blockchain_relay_fee) NOEXCEPT;

    /// Handlers (outputs).
    void handle_blockchain_utxo_get_address(const code& ec,
        rpc_interface::blockchain_utxo_get_address,
        const std::string& tx_hash, double index) NOEXCEPT;
    void handle_blockchain_outpoint_get_status(const code& ec,
        rpc_interface::blockchain_outpoint_get_status,
        const std::string& tx_hash, double index,
        const std::string& spk_hint) NOEXCEPT;
    void handle_blockchain_outpoint_subscribe(const code& ec,
        rpc_interface::blockchain_outpoint_subscribe,
        const std::string& tx_hash, double index,
        const std::string& spk_hint) NOEXCEPT;
    void handle_blockchain_outpoint_unsubscribe(const code& ec,
        rpc_interface::blockchain_outpoint_unsubscribe,
        const std::string& tx_hash, double index) NOEXCEPT;

    /// Handlers (addresses).
    void handle_blockchain_address_get_balance(const code& ec,
        rpc_interface::blockchain_address_get_balance,
        const std::string& address) NOEXCEPT;
    void handle_blockchain_address_get_history(const code& ec,
        rpc_interface::blockchain_address_get_history,
        const std::string& address) NOEXCEPT;
    void handle_blockchain_address_get_mempool(const code& ec,
        rpc_interface::blockchain_address_get_mempool,
        const std::string& address) NOEXCEPT;
    void handle_blockchain_address_list_unspent(const code& ec,
        rpc_interface::blockchain_address_list_unspent,
        const std::string& address) NOEXCEPT;
    void handle_blockchain_address_subscribe(const code& ec,
        rpc_interface::blockchain_address_subscribe,
        const std::string& address) NOEXCEPT;

    /// Handlers (scripthash).
    void handle_blockchain_scripthash_get_balance(const code& ec,
        rpc_interface::blockchain_scripthash_get_balance,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_get_history(const code& ec,
        rpc_interface::blockchain_scripthash_get_history,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_get_mempool(const code& ec,
        rpc_interface::blockchain_scripthash_get_mempool,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_list_unspent(const code& ec,
        rpc_interface::blockchain_scripthash_list_unspent,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_subscribe(const code& ec,
        rpc_interface::blockchain_scripthash_subscribe,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_unsubscribe(const code& ec,
        rpc_interface::blockchain_scripthash_unsubscribe,
        const std::string& scripthash) NOEXCEPT;

    /// Handlers (scriptpubkey).
    void handle_blockchain_scriptpubkey_get_balance(const code& ec,
        rpc_interface::blockchain_scriptpubkey_get_balance,
        const std::string& scriptpubkey) NOEXCEPT;
    void handle_blockchain_scriptpubkey_get_history(const code& ec,
        rpc_interface::blockchain_scriptpubkey_get_history,
        const std::string& scriptpubkey) NOEXCEPT;
    void handle_blockchain_scriptpubkey_get_mempool(const code& ec,
        rpc_interface::blockchain_scriptpubkey_get_mempool,
        const std::string& scriptpubkey) NOEXCEPT;
    void handle_blockchain_scriptpubkey_list_unspent(const code& ec,
        rpc_interface::blockchain_scriptpubkey_list_unspent,
        const std::string& scriptpubkey) NOEXCEPT;
    void handle_blockchain_scriptpubkey_subscribe(const code& ec,
        rpc_interface::blockchain_scriptpubkey_subscribe,
        const std::string& scriptpubkey) NOEXCEPT;
    void handle_blockchain_scriptpubkey_unsubscribe(const code& ec,
        rpc_interface::blockchain_scriptpubkey_unsubscribe,
        const std::string& scriptpubkey) NOEXCEPT;

    /// Handlers (transactions).
    void handle_blockchain_transaction_broadcast(const code& ec,
        rpc_interface::blockchain_transaction_broadcast,
        const std::string& raw_tx) NOEXCEPT;
    void handle_blockchain_transaction_broadcast_package(const code& ec,
        rpc_interface::blockchain_transaction_broadcast_package,
        const interface::value_t& raw_txs, bool verbose) NOEXCEPT;
    void handle_blockchain_transaction_get(const code& ec,
        rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
        bool verbose) NOEXCEPT;
    void handle_blockchain_transaction_get_merkle(const code& ec,
        rpc_interface::blockchain_transaction_get_merkle,
        const std::string& tx_hash, double height) NOEXCEPT;
    void handle_blockchain_transaction_id_from_position(const code& ec,
        rpc_interface::blockchain_transaction_id_from_position, double height,
        double tx_pos, bool merkle) NOEXCEPT;

    /// Handlers (server).
    void handle_server_add_peer(const code& ec,
        rpc_interface::server_add_peer,
        const interface::object_t& features) NOEXCEPT;
    void handle_server_banner(const code& ec,
        rpc_interface::server_banner) NOEXCEPT;
    void handle_server_donation_address(const code& ec,
        rpc_interface::server_donation_address) NOEXCEPT;
    void handle_server_features(const code& ec,
        rpc_interface::server_features) NOEXCEPT;
    void handle_server_peers_subscribe(const code& ec,
        rpc_interface::server_peers_subscribe) NOEXCEPT;
    void handle_server_ping(const code& ec,
        rpc_interface::server_ping) NOEXCEPT;

    /// See protocol_electrum_version.
    ////void handle_server_version(const code& ec,
    ////    rpc_interface::server_version, const std::string& client_name,
    ////    const interface::value_t& protocol_version) NOEXCEPT;

    /// Handlers (mempool).
    void handle_mempool_get_fee_histogram(const code& ec,
        rpc_interface::mempool_get_fee_histogram) NOEXCEPT;
    void handle_mempool_get_info(const code& ec,
        rpc_interface::mempool_get_info) NOEXCEPT;

protected:
    using hash_digest = system::hash_digest;
    enum class notify_t { address, scripthash, scriptpubkey };
    typedef std::function<void(const code&, const hash_digest&,
        const hash_digest&)> status_handler;

    /// Common implementation for block_header/s.
    void blockchain_block_headers(size_t starting, size_t quantity,
        size_t waypoint, bool multiplicity) NOEXCEPT;

    /// Completion handlers (for long-running address queries).
    /// -----------------------------------------------------------------------

    void get_balance(const hash_digest& hash) NOEXCEPT;
    void get_history(const hash_digest& hash) NOEXCEPT;
    void get_mempool(const hash_digest& hash) NOEXCEPT;
    void list_unspent(const hash_digest& hash) NOEXCEPT;

    void do_get_balance(const hash_digest& hash) NOEXCEPT;
    void do_get_history(const hash_digest& hash) NOEXCEPT;
    void do_get_mempool(const hash_digest& hash) NOEXCEPT;
    void do_list_unspent(const hash_digest& hash) NOEXCEPT;
    void do_status(const hash_digest& hash,
        const status_handler& sender) NOEXCEPT;

    void complete_get_balance(const code& ec, uint64_t confirmed,
        int64_t unconfirmed) NOEXCEPT;
    void complete_get_history(const code& ec,
        const database::histories& histories) NOEXCEPT;
    void complete_get_mempool(const code& ec,
        const database::histories& histories) NOEXCEPT;
    void complete_list_unspent(const code& ec,
        const database::unspents& unspents) NOEXCEPT;
    void complete_status(const code& ec, const hash_digest& hash,
        const hash_digest& status, const status_handler& sender) NOEXCEPT;

    void send_status(const code& ec, const hash_digest& hash,
        const hash_digest& status) NOEXCEPT;
    void notify_status(const code& ec, const hash_digest& hash,
        const hash_digest& status, notify_t type,
        node::address_t link) NOEXCEPT;

    /// Notification senders and send handlers.
    /// -----------------------------------------------------------------------

    void do_height(node::header_t link) NOEXCEPT;
    void do_header(node::header_t link) NOEXCEPT;
    void do_outpoint(node::header_t link) NOEXCEPT;
    void do_scripthash(node::address_t link) NOEXCEPT;

    /// Utilities.
    /// -----------------------------------------------------------------------

    inline bool at_least(server::electrum::version version) const NOEXCEPT
    {
        return channel_->version() >= version;
    }

    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

private:
    // Status hash optimization (~200 bytes).
    struct midstate
    {
        hash_digest status{};
        system::stream::out::fast stream{ status };
        system::hash::sha256::fast writer{ stream };
    };

    // Aliases.
    using array_t = network::rpc::array_t;
    using object_t = network::rpc::object_t;
    using version_t = protocol_electrum_version;
    static constexpr electrum::version minimum = version_t::minimum;
    static constexpr electrum::version maximum = version_t::maximum;

    // Address transformations.
    static array_t transform(const database::histories& histories) NOEXCEPT;
    static array_t transform(const database::unspents& unspents) NOEXCEPT;
    static std::string to_method_name(notify_t type) NOEXCEPT;

    // Compute server.features.hosts value from config.
    object_t self_hosts() const NOEXCEPT;
    array_t more_hosts() const NOEXCEPT;

    // Convert between legacy bitcoin payment address and scripthash.
    hash_digest extract_scripthash(const std::string& address) const NOEXCEPT;
    system::wallet::payment_address extract_address(
        const system::chain::script& script) const NOEXCEPT;

    // Validate a transaction given next block context.
    bool get_pool_context(system::chain::context& pool) const NOEXCEPT;
    code validate_tx(const system::chain::transaction& tx) const NOEXCEPT;
    code broadcast_tx(const system::chain::transaction::cptr& tx) NOEXCEPT;

    // Shared send/get implementations.
    void send_scripthash_unsubscribe(const hash_digest& hash) NOEXCEPT;
    void send_scripthash_subscribe(const hash_digest& hash) NOEXCEPT;
    bool send_outpoint_status(const system::chain::point& prevout,
        const std::string& spk_hint) NOEXCEPT;
    bool get_outpoint_status(object_t& status,
        const system::chain::point& prevout) const NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    const bool turbo_;
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    std::atomic_bool stopping_{};
    std::atomic_bool subscribed_height_{};
    std::atomic_bool subscribed_header_{};
    std::atomic_bool subscribed_outpoint_{};
    std::atomic_bool subscribed_scripthash_{};

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;

    // This is protected by strand.
    std::unordered_set<hash_digest> subscriptions_{};
};

} // namespace server
} // namespace libbitcoin

#endif
