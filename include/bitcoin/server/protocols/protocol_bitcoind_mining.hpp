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
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_mining>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_get_network_hash_ps(const code& ec,
        rpc_interface::get_network_hash_ps, double nblocks,
        double height) NOEXCEPT;
    bool handle_get_mining_info(const code& ec,
        rpc_interface::get_mining_info) NOEXCEPT;
    bool handle_submit_block(const code& ec,
        rpc_interface::submit_block, const std::string& hexdata,
        const std::string& dummy) NOEXCEPT;
    bool handle_submit_header(const code& ec,
        rpc_interface::submit_header, const std::string& hexdata) NOEXCEPT;
    bool handle_get_block_template(const code& ec,
        rpc_interface::get_block_template) NOEXCEPT;
    bool handle_get_prioritised_transactions(const code& ec,
        rpc_interface::get_prioritised_transactions) NOEXCEPT;
    bool handle_prioritise_transaction(const code& ec,
        rpc_interface::prioritise_transaction) NOEXCEPT;

private:
    /// Organize completions (bounced to the channel strand).
    void handle_organize_block(const code& ec, size_t height) NOEXCEPT;
    void handle_organize_header(const code& ec, size_t height) NOEXCEPT;
    void do_submit_block(const code& ec) NOEXCEPT;
    void do_submit_header(const code& ec) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
