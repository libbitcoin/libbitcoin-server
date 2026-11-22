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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_NETWORK_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_NETWORK_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_network.hpp>
#include <bitcoin/server/protocols/protocol_btcd_subgroup.hpp>

namespace libbitcoin {
namespace server {

/// The bitcoind network subgroup on the btcd endpoint, answering peers as
/// btcd does.
class BCS_API protocol_btcd_network
  : public protocol_btcd_subgroup<protocol_bitcoind_network>,
    protected network::tracker<protocol_btcd_network>
{
public:
    typedef std::shared_ptr<protocol_btcd_network> ptr;
    using base = protocol_btcd_subgroup<protocol_bitcoind_network>;

    inline protocol_btcd_network(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : base(session, channel, options),
        network::tracker<protocol_btcd_network>(session->log)
    {
    }

protected:
    void send_absent(const code& absent) NOEXCEPT override;
    void finish_peer(network::rpc::object_t& out,
        const network::diagnostics::row& row) const NOEXCEPT override;
};

} // namespace server
} // namespace libbitcoin

#endif
