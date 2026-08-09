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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_NOTIFICATIONS_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_NOTIFICATIONS_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_notifications>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_notifications>;

class BCS_API protocol_bitcoind_notifications
  : public protocol_bitcoind_dispatch<interface::bitcoind_notifications>,
    protected network::tracker<protocol_bitcoind_notifications>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_notifications> ptr;
    using rpc_interface = interface::bitcoind_notifications;

    inline protocol_bitcoind_notifications(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<interface::bitcoind_notifications>(session,
            channel, options),
        network::tracker<protocol_bitcoind_notifications>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_get_zmq_notifications(const code& ec,
        rpc_interface::get_zmq_notifications) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
