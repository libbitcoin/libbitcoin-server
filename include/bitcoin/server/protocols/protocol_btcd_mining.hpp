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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_MINING_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_MINING_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_mining.hpp>
#include <bitcoin/server/protocols/protocol_btcd_subgroup.hpp>

namespace libbitcoin {
namespace server {

/// bitcoind mining subgroup on btcd endpoint, answering mining state and
/// templates as btcd does.
class BCS_API protocol_btcd_mining
  : public protocol_btcd_subgroup<protocol_bitcoind_mining>,
    protected network::tracker<protocol_btcd_mining>
{
public:
    typedef std::shared_ptr<protocol_btcd_mining> ptr;
    using base = protocol_btcd_subgroup<protocol_bitcoind_mining>;

    inline protocol_btcd_mining(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : base(session, channel, options),
        network::tracker<protocol_btcd_mining>(session->log)
    {
    }

protected:
    void finish_template(network::rpc::object_t& out) const NOEXCEPT override;
    void do_mining_info(const code& ec, const database::tx_links& links,
        const gate_t::ptr& gate) NOEXCEPT override;
};

} // namespace server
} // namespace libbitcoin

#endif
