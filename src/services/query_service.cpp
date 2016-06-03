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
#include <bitcoin/server/services/query_service.hpp>

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::config;
using namespace bc::protocol;

static const auto domain = "query";
const config::endpoint query_service::public_worker("inproc://public_query");
const config::endpoint query_service::secure_worker("inproc://secure_query");

query_service::query_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    secure_(secure),
    settings_(node.server_settings()),
    authenticator_(authenticator)
{
}

// Implement worker as a broker.
// TODO: implement as a load balancing broker.
void query_service::work()
{
    zmq::socket router(authenticator_, zmq::socket::role::router);
    zmq::socket dealer(authenticator_, zmq::socket::role::dealer);

    // Bind sockets to the service and worker endpoints.
    if (!started(bind(router, dealer)))
        return;

    // Relay messages between router and dealer (blocks on context).
    relay(router, dealer);

    // Unbind the sockets and exit this thread.
    finished(unbind(router, dealer));
}

// Bind/Unbind.
//-----------------------------------------------------------------------------

bool query_service::bind(zmq::socket& router, zmq::socket& dealer)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& worker = secure_ ? secure_worker : public_worker;
    const auto& service = secure_ ? settings_.secure_query_endpoint :
        settings_.public_query_endpoint;

    if (secure_ && !authenticator_.apply(router, domain, true))
    {
        log::error(LOG_SERVER)
            << "Failed to apply authenticator to secure query service.";
        return false;
    }

    if (!router.bind(service))
    {
        log::error(LOG_SERVER)
            << "Failed to bind " << security << " query service to "
            << service;
        return false;
    }

    if (!dealer.bind(worker))
    {
        log::error(LOG_SERVER)
            << "Failed to bind " << security << " query workers to "
            << worker;
        return false;
    }

    log::info(LOG_SERVER)
        << "Bound " << security << " query service to " << service;
    return true;
}

bool query_service::unbind(zmq::socket& router, zmq::socket& dealer)
{
    // Stop both even if one fails.
    const auto router_stop = router.stop();
    const auto dealer_stop = dealer.stop();
    const auto security = secure_ ? "secure" : "public";

    if (!router_stop)
    {
        log::error(LOG_SERVER)
            << "Failed to unbind " << security << " query service.";
    }

    if (!dealer_stop)
    {
        log::error(LOG_SERVER)
            << "Failed to unbind " << security << " query workers.";
    }

    // Don't log stop success.
    return router_stop && dealer_stop;
}

} // namespace server
} // namespace libbitcoin
