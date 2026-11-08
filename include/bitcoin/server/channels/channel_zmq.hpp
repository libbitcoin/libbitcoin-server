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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ZMQ_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ZMQ_HPP

#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for zmq notifications, bitcoind compatible (rpc over native zmtp).
class BCS_API channel_zmq
  : public server::channel,
    public network::channel_rpc<interface::zmq>,
    protected network::tracker<channel_zmq>
{
public:
    typedef std::shared_ptr<channel_zmq> ptr;
    using interface_t = interface::zmq;
    using options_t = settings::zmq_server;

    /// The socket reads subscriptions and writes topic notifications.
    static constexpr auto role{ network::zmtp::role::publisher };

    inline channel_zmq(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_rpc<interface::zmq>(log, socket, identifier,
            config.network, options),
        network::tracker<channel_zmq>(log),
        options_(options)
    {
    }

    /// Service options.
    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

private:
    // This is thread safe.
    const options_t& options_;
};

} // namespace server
} // namespace libbitcoin

#endif
