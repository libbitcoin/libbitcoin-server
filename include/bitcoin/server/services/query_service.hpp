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
#ifndef LIBBITCOIN_SERVER_QUERY_SERVICE_HPP
#define LIBBITCOIN_SERVER_QUERY_SERVICE_HPP

#include <memory>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Submit queries and address subscriptions and receive address notifications.
class BCS_API query_service
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<query_service> ptr;

    /// A reference to each inprocess worker endpoint.
    static const config::endpoint& worker_endpoint(bool secure);

    /// Construct a query service.
    query_service(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    typedef bc::protocol::zmq::socket socket;

    virtual bool bind(socket& router, socket& dealer);
    virtual bool unbind(socket& router, socket& dealer);

    // Implement the service.
    virtual void work();

private:
    // These are thread safe.
    const bool secure_;
    const std::string security_;
    const bc::server::settings& settings_;
    const bc::protocol::settings& external_;
    const bc::protocol::settings internal_;
    const config::endpoint& service_;
    const config::endpoint& worker_;
    bc::protocol::zmq::authenticator& authenticator_;
};

} // namespace server
} // namespace libbitcoin

#endif
