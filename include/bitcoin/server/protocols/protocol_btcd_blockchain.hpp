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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_BLOCKCHAIN_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_BLOCKCHAIN_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_blockchain.hpp>
#include <bitcoin/server/protocols/protocol_btcd_subgroup.hpp>

namespace libbitcoin {
namespace server {

/// The bitcoind blockchain subgroup on the btcd endpoint, answering blocks,
/// outputs and the mempool as btcd does.
class BCS_API protocol_btcd_blockchain
  : public protocol_btcd_subgroup<protocol_bitcoind_blockchain>,
    protected network::tracker<protocol_btcd_blockchain>
{
public:
    typedef std::shared_ptr<protocol_btcd_blockchain> ptr;
    using base = protocol_btcd_subgroup<protocol_bitcoind_blockchain>;

    inline protocol_btcd_blockchain(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : base(session, channel, options),
        network::tracker<protocol_btcd_blockchain>(session->log)
    {
    }

protected:
    void inject_block(system::json::object& out,
        const database::header_link& link,
        const system::chain::header& header) const NOEXCEPT override;
    void inject_block_tx(system::json::object& out,
        const system::chain::transaction& tx, bool populated,
        bool prevouts) const NOEXCEPT override;
    void finish_block(system::json::object& out) const NOEXCEPT override;
    bool is_spent(const database::output_link& link,
        bool include_mempool) const NOEXCEPT override;
    bool verbose_mempool(system::json::value& out,
        const database::tx_links& links) const NOEXCEPT override;
    bool mempool_information(system::json::object& out,
        const database::fee_rates& rates, uint64_t minimum, uint64_t relay,
        uint64_t increment) const NOEXCEPT override;

    bool handle_get_block_hash(const code& ec,
        rpc_interface::get_block_hash, double height) NOEXCEPT override;
};

} // namespace server
} // namespace libbitcoin

#endif
