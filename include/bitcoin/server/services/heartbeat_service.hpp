/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_HEARTBEAT_SERVICE_HPP
#define LIBBITCOIN_SERVER_HEARTBEAT_SERVICE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Subscribe to a pulse from a dedicated service endpoint.
class BCS_API heartbeat_service
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<heartbeat_service> ptr;

    /// Construct a heartbeat endpoint.
    heartbeat_service(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    typedef bc::protocol::zmq::socket socket;

    virtual bool bind(socket& publisher);
    virtual bool unbind(socket& publisher);

    // Implement the service.
    virtual void work();

    // Publish the heartbeat (no worker).
    void publish(socket& socket);

private:
    int32_t pulse_milliseconds() const;

    // These are thread safe.
    const bool secure_;
    const std::string security_;
    const bc::server::settings& settings_;
    const bc::protocol::settings& external_;
    const config::endpoint service_;
    bc::protocol::zmq::authenticator& authenticator_;
    server_node& node_;

    // This is protected by limit to single worker thread.
    uint16_t sequence_;
};

} // namespace server
} // namespace libbitcoin

#endif
