/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_HEARTBEAT_ENDPONT_HPP
#define LIBBITCOIN_SERVER_HEARTBEAT_ENDPONT_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

/// This class must be constructed as a shared pointer.
class BCS_API heartbeat_endpoint
  : public enable_shared_from_base<heartbeat_endpoint>
{
public:
    typedef std::shared_ptr<heartbeat_endpoint> ptr;

    /// Construct a heartbeat endpoint.
    heartbeat_endpoint(bc::protocol::zmq::context& context, server_node* node);

    /// This class is not copyable.
    heartbeat_endpoint(const heartbeat_endpoint&) = delete;
    void operator=(const heartbeat_endpoint&) = delete;

    /// Start the heartbeat timer and send notifications.
    bool start();

    /// Stop the heartbeat timer.
    bool stop();

private:
    void start_timer();
    void send(const code& ec);

    uint32_t counter_;
    const settings& settings_;
    bc::protocol::zmq::socket socket_;
    deadline::ptr deadline_;
};

} // namespace server
} // namespace libbitcoin

#endif
