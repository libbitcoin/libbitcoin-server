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

#include <cstdint>
#include <functional>
#include <vector>
#include <boost/date_time.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/interface/address.hpp>
#include <bitcoin/server/interface/blockchain.hpp>
#include <bitcoin/server/interface/protocol.hpp>
#include <bitcoin/server/interface/transaction_pool.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/address_notifier.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define NAME "query_endpoint"
#define PUBLIC_NAME "public_query"
#define SECURE_NAME "secure_query"

using std::placeholders::_1;
using std::placeholders::_2;
using namespace bc::config;
using namespace bc::protocol;
using namespace boost::posix_time;

// Arbitrary inprocess endpoint for outbound queueing.
static const endpoint queue_endpoint("inproc://trigger-send");

static constexpr uint32_t polling_interval_milliseconds = 1;

static inline bool is_enabled(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return settings.query_endpoints_enabled &&
        (!secure || settings.server_private_key);
}

static inline config::endpoint get_endpoint(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return secure ? settings.secure_query_endpoint :
        settings.public_query_endpoint;
}

query_endpoint::query_endpoint(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : socket_(authenticator, zmq::socket::role::router),
    pull_socket_(context_, zmq::socket::role::puller),
    push_socket_(context_, zmq::socket::role::pusher),
    node_(node),
    dispatch_(node.thread_pool(), NAME),
    address_notifier_(node),
    endpoint_(get_endpoint(node, secure)),
    log_(node.server_settings().log_requests),
    enabled_(is_enabled(node, secure)),
    secure_(secure)
{
    const auto name = secure ? SECURE_NAME : PUBLIC_NAME;

    // The authenticator logs apply failures and stopped socket halts start.
    if (!enabled_ || !authenticator.apply(socket_, name, secure))
        socket_.stop();
}

// Start.
//-----------------------------------------------------------------------------

// The endpoint is not restartable.
bool query_endpoint::start()
{
    if (!enabled_)
        return true;

    if (!start_queue() || !start_endpoint() || !address_notifier_.start())
    {
        stop();
        return false;
    }

    attach_interface();
    return true;
}

bool query_endpoint::start_queue()
{
    if (!pull_socket_)
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize queue puller.";
        return false;
    }

    if (!push_socket_)
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize queue puller.";
        return false;
    }

    if (!pull_socket_.bind(queue_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize to queue puller.";
        return false;
    }

    if (!push_socket_.connect(queue_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize to queue pusher.";
        return false;
    }

    return true;
}

bool query_endpoint::start_endpoint()
{
    if (!socket_)
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize query service.";
        return false;
    }

    if (!socket_.bind(endpoint_))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to bind query service to " << endpoint_;
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound " << (secure_ ? "secure " : "public ")
        << "query service to " << endpoint_;

    dispatch_.concurrent(
        std::bind(&query_endpoint::monitor,
            this));

    return true;
}

// Stop.
//-----------------------------------------------------------------------------
 
bool query_endpoint::stop()
{
    const auto result = push_socket_.stop() && pull_socket_.stop() 
        && socket_.stop();

    log::debug(LOG_ENDPOINT)
        << "Unbound " << (secure_ ? "secure " : "public ")
        << "query service to " << endpoint_;

    return result;
}

// Monitors.
//-----------------------------------------------------------------------------

void query_endpoint::monitor()
{
    zmq::poller poller;
    poller.add(socket_);
    poller.add(pull_socket_);

    // Ignore expired and keep looping, exiting the thread when terminated.
    while (!poller.terminated())
    {
        const auto socket_id = poller.wait(polling_interval_milliseconds);

        if (socket_id == socket_.id())
            receive();
        else if (socket_id == pull_socket_.id())
            dequeue();
    }

    // When context is stopped the loop terminates, still need to stop sockets.
    stop();
}

// Utilities.
//-----------------------------------------------------------------------------

void query_endpoint::receive()
{
    incoming request;

    if (!request.receive(socket_))
    {
        log::info(LOG_ENDPOINT)
            << "Malformed request from "
            << encode_base16(request.address);
        return;
    }

    if (log_)
    {
        log::info(LOG_ENDPOINT)
            << "Command " << request.command << " from "
            << encode_base16(request.address);
    }

    // Locate the request handler for this command.
    const auto handler = handlers_.find(request.command);

    if (handler == handlers_.end())
    {
        log::info(LOG_ENDPOINT)
            << "Invalid command from "
            << encode_base16(request.address);
    }

    // Execute the request if a handler exists.
    handler->second(request,
        std::bind(&query_endpoint::enqueue,
            this, _1));
}

void query_endpoint::dequeue()
{
    zmq::message message;
    if (message.receive(pull_socket_) && message.send(socket_))
        return;

    log::debug(LOG_ENDPOINT)
        << "Failed to dequeue message.";
}

void query_endpoint::enqueue(outgoing& message)
{
    if (message.send(push_socket_))
        return;

    log::debug(LOG_ENDPOINT)
        << "Failed to enqueue message.";
}

// Query Interface.
// ----------------------------------------------------------------------------

// Class and method names must match protocol expectations (do not change).
#define ATTACH(class_name, method_name, instance) \
    attach(#class_name "." #method_name, \
        std::bind(&bc::server::class_name::method_name, \
            std::ref(instance), _1, _2));

void query_endpoint::attach(const std::string& command,
    command_handler handler)
{
    handlers_[command] = handler;
}

// Class and method names must match protocol expectations (do not change).
void query_endpoint::attach_interface()
{
    // TODO: add total_connections to client.
    ATTACH(protocol, total_connections, node_);
    ATTACH(protocol, broadcast_transaction, node_);
    ATTACH(transaction_pool, validate, node_);
    ATTACH(transaction_pool, fetch_transaction, node_);

    // TODO: add fetch_spend to client.
    // TODO: add fetch_block_transaction_hashes to client.
    ATTACH(blockchain, fetch_spend, node_);
    ATTACH(blockchain, fetch_transaction, node_);
    ATTACH(blockchain, fetch_last_height, node_);
    ATTACH(blockchain, fetch_block_header, node_);
    ATTACH(blockchain, fetch_block_height, node_);
    ATTACH(blockchain, fetch_transaction_index, node_);
    ATTACH(blockchain, fetch_stealth, node_);
    ATTACH(blockchain, fetch_history, node_);
    ATTACH(blockchain, fetch_block_transaction_hashes, node_);

    // address.fetch_history was present in v1 (obelisk) and v2 (server).
    // address.fetch_history was called by client v1 (sx) and v2 (bx).
    ////ATTACH(endpoint, address, fetch_history, node_);
    ATTACH(address, fetch_history2, node_);

    // TODO: add renew to client.
    ATTACH(address, renew, address_notifier_);
    ATTACH(address, subscribe, address_notifier_);
}

#undef ATTACH

} // namespace server
} // namespace libbitcoin
