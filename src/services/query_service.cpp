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
#include <bitcoin/server/services/query_service.hpp>

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::config;
using namespace bc::protocol;
using role = zmq::socket::role;

static const auto domain = "query";
static const config::endpoint public_worker("inproc://public_query");
static const config::endpoint secure_worker("inproc://secure_query");

// static
const config::endpoint& query_service::worker_endpoint(bool secure)
{
    return secure ? secure_worker : public_worker;
}

query_service::query_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    security_(secure ? "secure" : "public"),
    settings_(node.server_settings()),
    external_(node.protocol_settings()),
    internal_(external_.send_high_water, external_.receive_high_water),
    service_(settings_.query_endpoint(secure)),
    worker_(secure ? secure_worker : public_worker),
    authenticator_(authenticator)
{
}

// Implement worker as a broker.
// The dealer blocks until there are available workers.
// The router drops messages for lost peers (clients) and high water.
// ............................................................................
// When a ZMQ_ROUTER socket enters the mute state due to having reached
// the high water mark for all peers, then any messages sent to the socket
// shall be dropped until the mute state ends.Likewise, any messages routed
// to a peer for which the individual high water mark has been reached shall
// also be dropped. api.zeromq.org/4-2:zmq-socket
void query_service::work()
{
    zmq::socket router(authenticator_, role::router, external_);
    zmq::socket dealer(authenticator_, role::dealer, internal_);

    // Bind sockets to the service and worker endpoints.
    if (!started(bind(router, dealer)))
        return;

    // TODO: tap in to failure conditions, such as high water.
    // BUGBUG: stop is insufficient to stop the worker, because of relay().
    // Relay messages between router and dealer (blocks on context).
    relay(router, dealer);

    // Unbind the sockets and exit this thread.
    finished(unbind(router, dealer));
}

// Bind/Unbind.
//-----------------------------------------------------------------------------

bool query_service::bind(zmq::socket& router, zmq::socket& dealer)
{
    if (!authenticator_.apply(router, domain, secure_))
        return false;

    auto ec = router.bind(service_);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " query service to "
            << service_ << " : " << ec.message();
        return false;
    }

    ec = dealer.bind(worker_);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " query workers to "
            << worker_ << " : " << ec.message();
        return false;
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " query service to " << service_;
    return true;
}

bool query_service::unbind(zmq::socket& router, zmq::socket& dealer)
{
    // Stop both even if one fails.
    const auto service_stop = router.stop();
    const auto worker_stop = dealer.stop();

    if (!service_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " query service.";

    if (!worker_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " query workers.";

    // Don't log stop success.
    return service_stop && worker_stop;
}

} // namespace server
} // namespace libbitcoin
