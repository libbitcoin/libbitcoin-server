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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_TEST_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_TEST_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_test>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_test>;

class BCS_API protocol_bitcoind_test
  : public protocol_bitcoind_dispatch<interface::bitcoind_test>,
    protected network::tracker<protocol_bitcoind_test>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_test> ptr;
    using rpc_interface = interface::bitcoind_test;

    inline protocol_bitcoind_test(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_test>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    void handle_add_connection(const code& ec,
        rpc_interface::add_connection) NOEXCEPT;
    void handle_add_peer_address(const code& ec,
        rpc_interface::add_peer_address) NOEXCEPT;
    void handle_echo(const code& ec,
        rpc_interface::echo) NOEXCEPT;
    void handle_echo_ipc(const code& ec,
        rpc_interface::echo_ipc) NOEXCEPT;
    void handle_echo_json(const code& ec,
        rpc_interface::echo_json) NOEXCEPT;
    void handle_estimate_raw_fee(const code& ec,
        rpc_interface::estimate_raw_fee) NOEXCEPT;
    void handle_generate(const code& ec,
        rpc_interface::generate) NOEXCEPT;
    void handle_generate_block(const code& ec,
        rpc_interface::generate_block) NOEXCEPT;
    void handle_generate_to_address(const code& ec,
        rpc_interface::generate_to_address) NOEXCEPT;
    void handle_generate_to_descriptor(const code& ec,
        rpc_interface::generate_to_descriptor) NOEXCEPT;
    void handle_get_mempool_fee_rate_diagram(const code& ec,
        rpc_interface::get_mempool_fee_rate_diagram) NOEXCEPT;
    void handle_get_orphan_txs(const code& ec,
        rpc_interface::get_orphan_txs) NOEXCEPT;
    void handle_get_raw_addrman(const code& ec,
        rpc_interface::get_raw_addrman) NOEXCEPT;
    void handle_invalidate_block(const code& ec,
        rpc_interface::invalidate_block) NOEXCEPT;
    void handle_mock_scheduler(const code& ec,
        rpc_interface::mock_scheduler) NOEXCEPT;
    void handle_reconsider_block(const code& ec,
        rpc_interface::reconsider_block) NOEXCEPT;
    void handle_send_msg_to_peer(const code& ec,
        rpc_interface::send_msg_to_peer) NOEXCEPT;
    void handle_set_mock_time(const code& ec,
        rpc_interface::set_mock_time) NOEXCEPT;
    void handle_sync_with_validation_interface_queue(const code& ec,
        rpc_interface::sync_with_validation_interface_queue) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
