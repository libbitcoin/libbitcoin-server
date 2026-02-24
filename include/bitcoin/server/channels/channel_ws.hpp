/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_WS_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_WS_HPP

#include <memory>
#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class BCS_API channel_ws
  : public server::channel,
    public network::channel_ws,
    protected network::tracker<channel_ws>
{
public:
    typedef std::shared_ptr<channel_ws> ptr;

    /// Subscribe to messages post-upgrade (requires strand).
    /// Event handler is always invoked on the channel strand.
    template <class Message>
    inline void subscribe(auto&& ) NOEXCEPT
    {
        BC_ASSERT(stranded());
        ////using message_handler = distributor_ws::handler<Message>;
        ////ws_distributor_.subscribe(std::forward<message_handler>(handler));
    }
    
    /// Serialize and write websocket message to peer (requires strand).
    /// Completion handler is always invoked on the channel strand.
    inline void send(system::data_chunk&& message, bool binary,
        network::result_handler&& handler) NOEXCEPT
    {
        BC_ASSERT(stranded());
        using namespace std::placeholders;
    
        // TODO: Serialize message.
        const auto ptr = system::move_shared(std::move(message));
        network::count_handler complete = std::bind(&channel_ws::handle_send_ws,
            shared_from_base<channel_ws>(), _1, _2, ptr, std::move(handler));
    
        if (!ptr)
        {
            complete(network::error::bad_alloc, {});
            return;
        }
    
        // TODO: serialize message to send.
        // TODO: websocket is full duplex, so writes must be queued.
        ws_write(network::asio::const_buffer{ ptr->data(), ptr->size() },
            binary, std::move(complete));
    }

    inline channel_ws(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_ws(log, socket, identifier, config.network, options),
        network::tracker<channel_ws>(log)
    {
    }

protected:
    /// Dispatch websocket buffer via derived handlers (override to handle).
    /// Override to handle dispatch, must invoke read_request() on complete.
    inline void dispatch_ws(const network::http::flat_buffer&,
        size_t) NOEXCEPT override
    {
        const std::string welcome{ "Websocket libbitcoin/4.0" };
        send(system::to_chunk(welcome), false, [this](const code& ec) NOEXCEPT
        {
            // handle_send alread stops channel on ec.
            // One and only one handler of message must restart read loop.
            // In half duplex this happens only after send (ws full duplex).
            if (!ec) receive();
        });
    }

    inline void handle_send_ws(const code& ec, size_t, const system::chunk_ptr&,
        const network::result_handler& handler) NOEXCEPT
    {
        if (ec) stop(ec);
        handler(ec);
    }
};

} // namespace server
} // namespace libbitcoin

#endif
