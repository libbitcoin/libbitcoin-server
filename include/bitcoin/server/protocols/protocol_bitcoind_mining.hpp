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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_MINING_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_MINING_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_mining>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_mining>;

class BCS_API protocol_bitcoind_mining
  : public protocol_bitcoind_dispatch<interface::bitcoind_mining>,
    protected network::tracker<protocol_bitcoind_mining>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_mining> ptr;
    using rpc_interface = interface::bitcoind_mining;

    inline protocol_bitcoind_mining(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<interface::bitcoind_mining>(session,
            channel, options),
        network::tracker<protocol_bitcoind_mining>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_get_network_hash_ps(const code& ec,
        rpc_interface::get_network_hash_ps, uint32_t nblocks,
        int32_t height) NOEXCEPT;
    bool handle_get_mining_info(const code& ec,
        rpc_interface::get_mining_info) NOEXCEPT;
    bool handle_submit_block(const code& ec,
        rpc_interface::submit_block) NOEXCEPT;
    bool handle_submit_header(const code& ec,
        rpc_interface::submit_header) NOEXCEPT;
    bool handle_get_block_template(const code& ec,
        rpc_interface::get_block_template) NOEXCEPT;
    bool handle_get_prioritised_transactions(const code& ec,
        rpc_interface::get_prioritised_transactions) NOEXCEPT;
    bool handle_prioritise_transaction(const code& ec,
        rpc_interface::prioritise_transaction) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
