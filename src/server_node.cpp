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
#include <bitcoin/server/server_node.hpp>

#include <functional>
#include <future>
#include <memory>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/workers/query_worker.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::protocol;

server_node::server_node(const configuration& configuration)
  : p2p_node(configuration),
    configuration_(configuration),
    authenticator_(*this),
    secure_query_service_(authenticator_, *this, true),
    public_query_service_(authenticator_, *this, false),
    secure_heart_service_(authenticator_, *this, true),
    public_heart_service_(authenticator_, *this, false),
    secure_block_service_(authenticator_, *this, true),
    public_block_service_(authenticator_, *this, false),
    secure_trans_service_(authenticator_, *this, true),
    public_trans_service_(authenticator_, *this, false)
{
}

// This allows for shutdown based on destruct without need to call stop.
server_node::~server_node()
{
    server_node::close();
}
// Properties.
// ----------------------------------------------------------------------------

const settings& server_node::server_settings() const
{
    return configuration_.server;
}

// Run sequence.
// ----------------------------------------------------------------------------

void server_node::run(result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // The handler is invoked on a new thread.
    p2p_node::run(
        std::bind(&server_node::handle_running,
            this, _1, handler));
}

void server_node::handle_running(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Start authenticated context.
    if (!authenticator_.start())
    {
        handler(error::operation_failed);
        return;
    }

    // Start services and workers.
    if (!start_query_services() || !start_heart_services() ||
        !start_block_services() || !start_trans_services())
    {
        handler(error::operation_failed);
        return;
    }

    // This is the end of the derived run sequence.
    handler(error::success);
}

// Shutdown.
// ----------------------------------------------------------------------------

bool server_node::stop()
{
    // Suspend new work last so we can use work to clear subscribers.
    return authenticator_.stop() && p2p_node::stop();
}

// This must be called from the thread that constructed this class (see join).
bool server_node::close()
{
    // Invoke own stop to signal work suspension, then close the node.
    return server_node::stop() && p2p_node::close();
}

// Services.
// ----------------------------------------------------------------------------

// The number of threads available in the thread pool must be sufficient
// to allocate the workers. This will wait forever on thread availability.
bool server_node::start_query_workers(bool secure)
{
    auto& server = *this;
    const auto& settings = configuration_.server;

    for (auto count = 0; count < settings.query_workers; ++count)
    {
        auto worker = std::make_shared<query_worker>(authenticator_,
            server, secure);

        if (!worker->start())
            return false;

        subscribe_stop([=](const code&) { worker->stop(); });
    }

    return true;
}

bool server_node::start_query_services()
{
    const auto& settings = configuration_.server;

    if (!settings.query_service_enabled || settings.query_workers == 0)
        return true;

    // Start secure service and workers if enabled.
    if (settings.server_private_key && (!secure_query_service_.start() ||
        !start_query_workers(true)))
            return false;

    // Start public service and workers if enabled.
    if (!settings.secure_only && (!public_query_service_.start() ||
        !start_query_workers(false)))
            return false;

    return true;
}

bool server_node::start_heart_services()
{
    const auto& settings = configuration_.server;

    if (!settings.heartbeat_service_enabled ||
        settings.heartbeat_interval_seconds == 0)
        return true;

    // Start public service if enabled.
    if (settings.server_private_key && !secure_heart_service_.start())
        return false;

    // Start public service if enabled.
    if (!settings.secure_only && !public_heart_service_.start())
        return false;

    return true;
}

bool server_node::start_block_services()
{
    // TODO:
    return true;
}

bool server_node::start_trans_services()
{
    // TODO:
    return true;
}
} // namespace server
} // namespace libbitcoin
