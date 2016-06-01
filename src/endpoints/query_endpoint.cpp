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
#include <bitcoin/server/endpoints/query_endpoint.hpp>

#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/workers/query_worker.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::config;
using namespace bc::protocol;

query_endpoint::query_endpoint(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    secure_(secure),
    enabled_(is_enabled(node, secure)),
    foreground_(get_foreground(node, secure)),
    authenticator_(authenticator)
{
}

bool query_endpoint::start()
{
    if (!enabled_)
        return true;

    if (!zmq::worker::start())
    {
        log::error(LOG_SERVER)
            << "Failed to bind query service to " << foreground_;
        return false;
    }

    log::info(LOG_SERVER)
        << "Bound " << (secure_ ? "secure " : "public ")
        << "query service to " << foreground_;
    return true;
}

bool query_endpoint::stop()
{
    if (!zmq::worker::stop())
    {
        log::error(LOG_SERVER)
            << "Failed to unbind query service from " << foreground_;
        return false;
    }

    log::debug(LOG_SERVER)
        << "Unbound " << (secure_ ? "secure " : "public ")
        << "query service from " << foreground_;
    return true;
}

// api.zeromq.org/3-2:zmq-proxy
// Implement worker as a broker (shared queue proxy). 
void query_endpoint::work()
{
    zmq::socket router(authenticator_, zmq::socket::role::router);
    zmq::socket dealer(authenticator_, zmq::socket::role::dealer);

    const auto result = 
        authenticator_.apply(router, get_domain(true, secure_), secure_) &&
        authenticator_.apply(dealer, get_domain(false, secure_), false) &&
        dealer.bind(query_worker::endpoint) && router.bind(foreground_);

    if (!started(result))
        return;

    // Relay messages between router and dealer (blocks on context).
    relay(router, dealer);

    // Stop the sockets and exit this thread.
    finished(router.stop() && dealer.stop());
}

// Utilities.
//-----------------------------------------------------------------------------

std::string query_endpoint::get_domain(bool service, bool secure)
{
    const std::string prefix = secure ? "secure" : "public";
    const std::string suffix = service ? "service" : "worker";
    return prefix + "_query_" + suffix;
}

config::endpoint query_endpoint::get_foreground(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return secure ? settings.secure_query_endpoint :
        settings.public_query_endpoint;
}

bool query_endpoint::is_enabled(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return settings.query_endpoints_enabled &&
        (!secure || settings.server_private_key);
}

} // namespace server
} // namespace libbitcoin
