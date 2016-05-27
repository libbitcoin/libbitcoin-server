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
#ifndef LIBBITCOIN_SERVER_TRANSACTION_ENDPOINT_HPP
#define LIBBITCOIN_SERVER_TRANSACTION_ENDPOINT_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

class server_node;

/// This class must be constructed as a shared pointer.
class BCS_API transaction_endpoint
  : public enable_shared_from_base<transaction_endpoint>
{
public:
    typedef std::shared_ptr<transaction_endpoint> ptr;

    /// Construct a transaction endpoint.
    transaction_endpoint(bc::protocol::zmq::authenticator& authenticator,
        server_node* node);

    /// This class is not copyable.
    transaction_endpoint(const transaction_endpoint&) = delete;
    void operator=(const transaction_endpoint&) = delete;

    /// Subscribe to transaction notifications and relay transactions.
    bool start();

    /// Stop the socket.
    bool stop();

private:
    void send(const chain::transaction& tx);

    server_node* node_;
    bc::protocol::zmq::socket socket_;
    const settings& settings_;
};

} // namespace server
} // namespace libbitcoin

#endif
