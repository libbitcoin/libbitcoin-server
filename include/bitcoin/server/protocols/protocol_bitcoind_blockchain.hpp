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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_BLOCKCHAIN_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_BLOCKCHAIN_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_blockchain>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_blockchain>;

class BCS_API protocol_bitcoind_blockchain
  : public protocol_bitcoind_dispatch<interface::bitcoind_blockchain>,
    protected network::tracker<protocol_bitcoind_blockchain>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_blockchain> ptr;
    using rpc_interface = interface::bitcoind_blockchain;

    inline protocol_bitcoind_blockchain(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_blockchain>(session->log),
        wait_timer_(std::make_shared<network::deadline>(session->log,
            channel->strand()))
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
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
        rpc_interface::get_tx_out_set_info, const std::string&,
        const network::rpc::value_t&, bool) NOEXCEPT;
    bool handle_prune_block_chain(const code& ec,
        rpc_interface::prune_block_chain, double) NOEXCEPT;
    bool handle_save_mempool(const code& ec,
        rpc_interface::save_mempool) NOEXCEPT;
    bool handle_scan_tx_out_set(const code& ec,
        rpc_interface::scan_tx_out_set, const std::string&,
        const network::rpc::array_t&) NOEXCEPT;
    bool handle_verify_chain(const code& ec,
        rpc_interface::verify_chain, double, double) NOEXCEPT;
    bool handle_dump_tx_out_set(const code& ec,
        rpc_interface::dump_tx_out_set) NOEXCEPT;
    bool handle_load_tx_out_set(const code& ec,
        rpc_interface::load_tx_out_set) NOEXCEPT;
    bool handle_get_tx_out_proof(const code& ec,
        rpc_interface::get_tx_out_proof, const network::rpc::array_t& txids,
        const std::string& blockhash) NOEXCEPT;
    bool handle_verify_tx_out_proof(const code& ec,
        rpc_interface::verify_tx_out_proof, const std::string& proof) NOEXCEPT;
    bool handle_get_block_from_peer(const code& ec,
        rpc_interface::get_block_from_peer) NOEXCEPT;
    bool handle_get_chain_states(const code& ec,
        rpc_interface::get_chain_states) NOEXCEPT;
    bool handle_get_chain_tips(const code& ec,
        rpc_interface::get_chain_tips) NOEXCEPT;
    bool handle_get_deployment_info(const code& ec,
        rpc_interface::get_deployment_info,
        const std::string& blockhash) NOEXCEPT;
    bool handle_get_descriptor_activity(const code& ec,
        rpc_interface::get_descriptor_activity,
        const network::rpc::array_t& blockhashes,
        const network::rpc::array_t& scanobjects,
        bool include_mempool) NOEXCEPT;
    bool handle_get_difficulty(const code& ec,
        rpc_interface::get_difficulty) NOEXCEPT;
    bool handle_precious_block(const code& ec,
        rpc_interface::precious_block) NOEXCEPT;
    bool handle_scan_blocks(const code& ec,
        rpc_interface::scan_blocks, const std::string& action,
        const network::rpc::array_t& scanobjects, double start_height,
        double stop_height, const std::string& filtertype,
        const network::rpc::object_t& options) NOEXCEPT;
    bool handle_wait_for_block(const code& ec,
        rpc_interface::wait_for_block, const std::string& blockhash,
        double timeout) NOEXCEPT;
    bool handle_wait_for_block_height(const code& ec,
        rpc_interface::wait_for_block_height, double height,
        double timeout) NOEXCEPT;
    bool handle_wait_for_new_block(const code& ec,
        rpc_interface::wait_for_new_block, double timeout,
        const std::string& current_tip) NOEXCEPT;
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

    /// Chase events (block wait long polling).
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;

private:
    enum class wait : uint8_t { none, new_block, block, height };

    void arm_wait(double timeout) NOEXCEPT;
    void do_wait_event() NOEXCEPT;
    void handle_wait_timeout(const code& ec) NOEXCEPT;
    bool wait_done() const NOEXCEPT;
    void send_tip() NOEXCEPT;

    enum class set_hash : uint8_t { none, muhash, serialized };

    void do_get_tx_out_set_info(set_hash type, size_t height) NOEXCEPT;
    void do_scan_tx_out_set(
        const std::shared_ptr<network::rpc::array_t>& objects) NOEXCEPT;
    void complete_scan(const code& ec, network::rpc::object_t& result,
        size_t size) NOEXCEPT;

    // This is thread safe.
    std::atomic_bool stopping_{};

    // These are protected by strand.
    wait wait_{ wait::none };
    size_t wait_height_{};
    system::hash_digest wait_hash_{};
    network::deadline::ptr wait_timer_;
};

} // namespace server
} // namespace libbitcoin

#endif
