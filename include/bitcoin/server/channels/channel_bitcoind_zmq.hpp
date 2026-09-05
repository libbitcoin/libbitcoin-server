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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BITCOIND_ZMQ_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BITCOIND_ZMQ_HPP

#include <memory>
#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for bitcoind zmq notifications (native zmtp publisher, framed).
class BCS_API channel_bitcoind_zmq
  : public server::channel,
    public network::channel,
    protected network::tracker<channel_bitcoind_zmq>
{
public:
    typedef std::shared_ptr<channel_bitcoind_zmq> ptr;
    using options_t = settings::bitcoind_zmq_server;
    using frame_t = network::zmtp::stream::frame;

    inline channel_bitcoind_zmq(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        options_(options),
        network::channel(log, socket, identifier, config.network, options),
        network::tracker<channel_bitcoind_zmq>(log)
    {
    }

    /// Service options.
    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

    /// Read the next frame from the subscriber (requires strand). The proxy
    /// absorbs keepalive, so frames delivered here are subscriptions.
    inline void read_frame(frame_t& out,
        network::count_handler&& handler) NOEXCEPT
    {
        network::proxy::read(out, std::move(handler));
    }

    /// Write a framed notification (see zmtp::stream::frame_message).
    inline void write_packet(const system::chunk_cptr& packet,
        network::count_handler&& handler) NOEXCEPT
    {
        network::proxy::write(packet, std::move(handler));
    }

private:
    const options_t& options_;
};

} // namespace server
} // namespace libbitcoin

#endif
