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
#ifndef LIBBITCOIN_SERVER_QUERY_SERVICE_HPP
#define LIBBITCOIN_SERVER_QUERY_SERVICE_HPP

#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

class BCS_API query_service
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<query_service> ptr;

    /// The fixed inprocess worker endpoints.
    static const config::endpoint public_worker;
    static const config::endpoint secure_worker;

    /// Construct a query service.
    query_service(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

protected:
    virtual bool bind(bc::protocol::zmq::socket& router,
        bc::protocol::zmq::socket& dealer);
    virtual bool unbind(bc::protocol::zmq::socket& router,
        bc::protocol::zmq::socket& dealer);

    // Implement the service.
    virtual void work();

private:
    const bool secure_;
    const server::settings& settings_;

    // This is thread safe.
    bc::protocol::zmq::authenticator& authenticator_;
};

} // namespace server
} // namespace libbitcoin

#endif
