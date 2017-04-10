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
#include <bitcoin/server/server_node.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/route.hpp>
#include <bitcoin/server/workers/query_worker.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::protocol;

server_node::server_node(const configuration& configuration)
  : full_node(configuration),
    configuration_(configuration),
    authenticator_(*this),
    secure_query_service_(authenticator_, *this, true),
    public_query_service_(authenticator_, *this, false),
    secure_heartbeat_service_(authenticator_, *this, true),
    public_heartbeat_service_(authenticator_, *this, false),
    secure_block_service_(authenticator_, *this, true),
    public_block_service_(authenticator_, *this, false),
    secure_transaction_service_(authenticator_, *this, true),
    public_transaction_service_(authenticator_, *this, false),
    secure_notification_worker_(authenticator_, *this, true),
    public_notification_worker_(authenticator_, *this, false)
{
}

// This allows for shutdown based on destruct without need to call stop.
server_node::~server_node()
{
    server_node::close();
}

// Properties.
// ----------------------------------------------------------------------------

const bc::protocol::settings& server_node::protocol_settings() const
{
    return configuration_.protocol;
}

const bc::server::settings& server_node::server_settings() const
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
    full_node::run(
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

    // BUGBUG: start/stop race condition.
    // This can invoke just after close calls stop.
    // The stop handler is already stopped but the authenticator context gets
    // started, allowing services to stop. The registration of services with
    // the stop handler invokes the registered handlers immediately, invoking
    // stop o nthe services. The services are running and don't stop...
    // notification_worker, query_service and authenticator service.
    // The authenticator is already stopped (before it started) so there will
    // be no context stop to stop the services, specifically the relays.
    if (!start_services())
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
    return authenticator_.stop() && full_node::stop();
}

// This must be called from the thread that constructed this class (see join).
bool server_node::close()
{
    // Invoke own stop to signal work suspension, then close node and join.
    return server_node::stop() && full_node::close();
}

// Notification.
// ----------------------------------------------------------------------------

code server_node::subscribe_address(const message& request,
    short_hash&& address_hash, bool unsubscribe)
{
    return request.secure() ?
        secure_notification_worker_.subscribe_address(request,
            std::move(address_hash), unsubscribe) :
        public_notification_worker_.subscribe_address(request,
            std::move(address_hash), unsubscribe);
}

code server_node::subscribe_stealth(const message& request,
    binary&& prefix_filter, bool unsubscribe)
{
    return request.secure() ?
        secure_notification_worker_.subscribe_stealth(request,
            std::move(prefix_filter), unsubscribe) :
        public_notification_worker_.subscribe_stealth(request,
            std::move(prefix_filter), unsubscribe);
}

// Services.
// ----------------------------------------------------------------------------

bool server_node::start_services()
{
    return
        start_authenticator() && start_query_services() &&
        start_heartbeat_services() && start_block_services() &&
        start_transaction_services();
}

bool server_node::start_authenticator()
{
    const auto& settings = configuration_.server;

    // Subscriptions require the query service.
    if ((!settings.server_private_key && settings.secure_only) ||
        ((settings.query_workers == 0) &&
        (settings.heartbeat_service_seconds == 0) &&
        (!settings.block_service_enabled) &&
        (!settings.transaction_service_enabled)))
        return true;

    return authenticator_.start();
}

bool server_node::start_query_services()
{
    const auto& settings = configuration_.server;

    // Subscriptions require the query service.
    if (settings.query_workers == 0)
        return true;

    // Start secure service, query workers and notification workers if enabled.
    if (settings.server_private_key &&
        (!secure_query_service_.start() || !start_query_workers(true) ||
        (settings.subscription_limit > 0 && !start_notification_workers(true))))
            return false;

    // Start public service, query workers and notification workers if enabled.
    if (!settings.secure_only && 
        (!public_query_service_.start() || !start_query_workers(false) ||
        (settings.subscription_limit > 0 && !start_notification_workers(false))))
            return false;

    return true;
}

bool server_node::start_heartbeat_services()
{
    const auto& settings = configuration_.server;

    if (settings.heartbeat_service_seconds == 0)
        return true;

    // Start secure service if enabled.
    if (settings.server_private_key && !secure_heartbeat_service_.start())
        return false;

    // Start public service if enabled.
    if (!settings.secure_only && !public_heartbeat_service_.start())
        return false;

    return true;
}

bool server_node::start_block_services()
{
    const auto& settings = configuration_.server;

    if (!settings.block_service_enabled)
        return true;

    // Start secure service if enabled.
    if (settings.server_private_key && !secure_block_service_.start())
        return false;

    // Start public service if enabled.
    if (!settings.secure_only && !public_block_service_.start())
        return false;

    return true;
}

bool server_node::start_transaction_services()
{
    const auto& settings = configuration_.server;

    if (!settings.transaction_service_enabled)
        return true;

    // Start secure service if enabled.
    if (settings.server_private_key && !secure_transaction_service_.start())
        return false;

    // Start public service if enabled.
    if (!settings.secure_only && !public_transaction_service_.start())
        return false;

    return true;
}

// Called from start_query_services.
bool server_node::start_query_workers(bool secure)
{
    auto& server = *this;
    const auto& settings = configuration_.server;

    for (auto count = 0; count < settings.query_workers; ++count)
    {
        const auto worker = std::make_shared<query_worker>(authenticator_,
            server, secure);

        if (!worker->start())
            return false;

        // Workers register with stop handler just to keep them in scope.
        subscribe_stop([=](const code&) { worker->stop(); });
    }

    return true;
}

// Called from start_query_services.
bool server_node::start_notification_workers(bool secure)
{
    auto& server = *this;
    const auto& settings = configuration_.server;

    if (secure)
    {
        if (!secure_notification_worker_.start())
            return false;

        // Because the notification worker holds closures must stop early.
        subscribe_stop([=](const code&)
        {
            secure_notification_worker_.stop();
        });
    }
    else
    {
        if (!public_notification_worker_.start())
            return false;

        // Because the notification worker holds closures must stop early.
        subscribe_stop([=](const code&)
        {
            public_notification_worker_.stop();
        });
    }

    return true;
}

} // namespace server
} // namespace libbitcoin
