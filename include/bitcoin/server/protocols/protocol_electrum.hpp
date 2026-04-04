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
#include <unordered_map>
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
        rpc_interface::blockchain_headers_subscribe) NOEXCEPT;

    /// Handlers (fees).
    void handle_blockchain_estimate_fee(const code& ec,
        rpc_interface::blockchain_estimate_fee, double number,
        const std::string& mode) NOEXCEPT;
    void handle_blockchain_relay_fee(const code& ec,
        rpc_interface::blockchain_relay_fee) NOEXCEPT;

    /// Handlers (addresses).
    void handle_blockchain_utxo_get_address(const code& ec,
        rpc_interface::blockchain_utxo_get_address,
        const std::string& tx_hash, double index) NOEXCEPT;
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
    /// Common implementation for block_header/s.
    void blockchain_block_headers(size_t starting, size_t quantity,
        size_t waypoint, bool multiplicity) NOEXCEPT;

    /// Notify client of newly organized block.
    void do_height(node::header_t link) NOEXCEPT;
    void do_header(node::header_t link) NOEXCEPT;

    inline bool at_least(server::electrum::version version) const NOEXCEPT
    {
        return channel_->version() >= version;
    }

    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

private:
    using version_t = protocol_electrum_version;
    static constexpr electrum::version minimum = version_t::minimum;
    static constexpr electrum::version maximum = version_t::maximum;

    // Compute server.features.hosts value from config.
    network::rpc::object_t self_hosts() const NOEXCEPT;
    network::rpc::array_t more_hosts() const NOEXCEPT;

    // Extract the legacy bitcoin payment address of a script.
    system::wallet::payment_address extract_address(
        const system::chain::script& script) NOEXCEPT;

    // Validate a transaction given next block context.
    bool get_pool_context(system::chain::context& pool) const NOEXCEPT;
    code validate_tx(const system::chain::transaction& tx) const NOEXCEPT;
    code broadcast_tx(const system::chain::transaction::cptr& tx) NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    std::atomic_bool subscribed_height_{};
    std::atomic_bool subscribed_header_{};

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;
};

} // namespace server
} // namespace libbitcoin

#endif
