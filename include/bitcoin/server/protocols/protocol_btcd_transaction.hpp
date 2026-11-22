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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_TRANSACTION_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_TRANSACTION_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_transaction.hpp>

namespace libbitcoin {
namespace server {

/// The bitcoind transaction subgroup on the btcd endpoint, answering
/// submissions as btcd does.
class BCS_API protocol_btcd_transaction
  : public protocol_bitcoind_transaction,
    protected network::tracker<protocol_btcd_transaction>
{
public:
    typedef std::shared_ptr<protocol_btcd_transaction> ptr;

    inline protocol_btcd_transaction(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_transaction(session, channel, options),
        network::tracker<protocol_btcd_transaction>(session->log)
    {
    }

protected:
    void send_rejected(const code& ec,
        const system::chain::transaction& tx) NOEXCEPT override;
    network::rpc::object_t test_result(const code& ec,
        const system::chain::transaction& tx,
        double maxfeerate) NOEXCEPT override;

private:
    bool is_orphan(const code& ec,
        const system::chain::transaction& tx) const NOEXCEPT;
    system::hash_digest missing_parent(
        const system::chain::transaction& tx) const NOEXCEPT;
    error::btcd::error_t rejection(std::string& out, const code& ec,
        const system::chain::transaction& tx) const NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
