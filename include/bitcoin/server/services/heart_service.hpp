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
#ifndef LIBBITCOIN_SERVER_HEART_SERVICE_HPP
#define LIBBITCOIN_SERVER_HEART_SERVICE_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class server_node;

class BCS_API heart_service
  : public enable_shared_from_base<heart_service>
{
public:
    typedef std::shared_ptr<heart_service> ptr;

    /// Construct a heartbeat endpoint.
    heart_service(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// This class is not copyable.
    heart_service(const heart_service&) = delete;
    void operator=(const heart_service&) = delete;

    /// Start the endpoint.
    bool start();

    /// Stop the endpoint.
    /// Stopping the authenticated context does not stop the publisher.
    bool stop();

private:
    void publisher(std::promise<code>& started);
    void send(uint32_t count, bc::protocol::zmq::socket& socket);

    // These are protected by mutex.
    bc::protocol::zmq::authenticator& authenticator_;
    dispatcher dispatch_;
    std::atomic<bool> stopped_;
    std::promise<code> stopping_;
    mutable shared_mutex mutex_;

    const bc::config::endpoint endpoint_;
    const std::chrono::seconds interval_;
    const bool log_;
    const bool enabled_;
    const bool secure_;
};

} // namespace server
} // namespace libbitcoin

#endif
