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
#ifndef LIBBITCOIN_SERVER_QUERY_ENDPOINT_HPP
#define LIBBITCOIN_SERVER_QUERY_ENDPOINT_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class server_node;

class BCS_API query_endpoint
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<query_endpoint> ptr;

    /// The fixed inprocess workers endpoint.
    static const config::endpoint workers;

    /// Construct a query service.
    query_endpoint(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// Start the service (restartable).
    bool start() override;

    /// Stop the service (idempotent).
    bool stop() override;

private:
    static std::string get_domain(bool service, bool secure);
    static config::endpoint get_service(server_node& node, bool secure);
    static bool is_enabled(server_node& node, bool secure);

    // Implement the service.
    virtual void work();

    const bool secure_;
    const bool enabled_;
    const bc::config::endpoint service_;

    // This is thread safe.
    bc::protocol::zmq::authenticator& authenticator_;
};

} // namespace server
} // namespace libbitcoin

#endif
