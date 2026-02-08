/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        network::tracker<protocol_electrum>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers (blockchain).
    void handle_blockchain_block_header(const code& ec,
        rpc_interface::blockchain_block_header, double height,
        double cp_height) NOEXCEPT;
    void handle_blockchain_block_headers(const code& ec,
        rpc_interface::blockchain_block_headers, double start_height,
        double count, double cp_height) NOEXCEPT;
    void handle_blockchain_headers_subscribe(const code& ec,
        rpc_interface::blockchain_headers_subscribe) NOEXCEPT;
    void handle_blockchain_estimate_fee(const code& ec,
        rpc_interface::blockchain_estimate_fee, double number,
        const std::string& mode) NOEXCEPT;
    void handle_blockchain_relay_fee(const code& ec,
        rpc_interface::blockchain_relay_fee) NOEXCEPT;
    void handle_blockchain_scripthash_get_balance(const code& ec,
        rpc_interface::blockchain_scripthash_get_balance,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_get_history(const code& ec,
        rpc_interface::blockchain_scripthash_get_history,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_get_mempool(const code& ec,
        rpc_interface::blockchain_scripthash_get_mempool,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_listunspent(const code& ec,
        rpc_interface::blockchain_scripthash_listunspent,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_subscribe(const code& ec,
        rpc_interface::blockchain_scripthash_subscribe,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_scripthash_unsubscribe(const code& ec,
        rpc_interface::blockchain_scripthash_unsubscribe,
        const std::string& scripthash) NOEXCEPT;
    void handle_blockchain_transaction_broadcast(const code& ec,
        rpc_interface::blockchain_transaction_broadcast,
        const std::string& raw_tx) NOEXCEPT;
    void handle_blockchain_transaction_get(const code& ec,
        rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
        bool verbose) NOEXCEPT;
    void handle_blockchain_transaction_get_merkle(const code& ec,
        rpc_interface::blockchain_transaction_get_merkle,
        const std::string& tx_hash, double height) NOEXCEPT;
    void handle_blockchain_transaction_id_from_pos(const code& ec,
        rpc_interface::blockchain_transaction_id_from_pos, double height,
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
    ////void handle_server_version(const code& ec,
    ////    rpc_interface::server_version, const std::string& client_name,
    ////    const interface::value_t& protocol_version) NOEXCEPT;

    /// Handlers (mempool).
    void handle_mempool_get_fee_histogram(const code& ec,
        rpc_interface::mempool_get_fee_histogram) NOEXCEPT;

protected:
    inline bool is_version(electrum_version version) const NOEXCEPT
    {
        return channel_->version() >= version;
    }

private:
    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;
};

} // namespace server
} // namespace libbitcoin

#endif
