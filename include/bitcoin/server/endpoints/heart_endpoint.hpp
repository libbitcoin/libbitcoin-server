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
#ifndef LIBBITCOIN_SERVER_HEART_ENDPOINT_HPP
#define LIBBITCOIN_SERVER_HEART_ENDPOINT_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

class server_node;

/// This class must be constructed as a shared pointer.
class BCS_API heart_endpoint
  : public enable_shared_from_base<heart_endpoint>
{
public:
    typedef std::shared_ptr<heart_endpoint> ptr;

    /// Construct a heartbeat endpoint.
    heart_endpoint(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// This class is not copyable.
    heart_endpoint(const heart_endpoint&) = delete;
    void operator=(const heart_endpoint&) = delete;

    /// Start the heartbeat timer and send notifications.
    bool start();

    /// Stop the heartbeat timer.
    bool stop();

private:
    void start_timer();
    void send(const code& ec);

    uint32_t counter_;
    bc::protocol::zmq::socket socket_;
    deadline::ptr deadline_;
    const bc::config::endpoint endpoint_;
    const bool enabled_;
    const bool secure_;
};

} // namespace server
} // namespace libbitcoin

#endif
