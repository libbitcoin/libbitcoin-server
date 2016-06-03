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
#include <bitcoin/server/workers/query_worker.hpp>

#include <functional>
#include <string>
#include <boost/thread.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/interface/address.hpp>
#include <bitcoin/server/interface/blockchain.hpp>
#include <bitcoin/server/interface/protocol.hpp>
#include <bitcoin/server/interface/transaction_pool.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;

query_worker::query_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(node.thread_pool()),
    log_(node.server_settings().log_requests),
    secure_(secure),
    settings_(node.server_settings()),
    authenticator_(authenticator),
    address_notifier_(node)
{
}

// Implement worker as a replier.
// A replier requires strict receive-send ordering.
// NOTE: v2 libbitcoin-client DEALER does not add delimiter frame.
void query_worker::work()
{
    //// TODO: change back to replier once response is implemented.
    zmq::socket replier(authenticator_, zmq::socket::role::router);

    // Connect socket to the worker endpoint.
    if (!started(connect(replier)))
        return;

    zmq::poller poller;
    poller.add(replier);
    
    while (!poller.terminated() && !stopped())
    {
        if (!poller.wait().contains(replier.id()))
            continue;

        zmq::message request;
        auto result = request.receive(replier);
        // Process request->data.
        ////outgoing response(request, error::success);
        ////result = response.send(replier);
    }

    // Disconnect the socket and exit this thread.
    finished(disconnect(replier));
}

// Connect/Disconnect.
//-----------------------------------------------------------------------------

bool query_worker::connect(zmq::socket& replier)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? query_service::secure_worker :
        query_service::public_worker;

    if (!replier.connect(endpoint))
    {
        log::error(LOG_SERVER)
            << "Failed to connect " << security << " query worker to "
            << endpoint;
        return false;
    }

    log::debug(LOG_SERVER)
        << "Connected " << security << " query worker to " << endpoint;
    return true;
}

bool query_worker::disconnect(zmq::socket& replier)
{
    const auto security = secure_ ? "secure" : "public";

    if (!replier.stop())
    {
        log::error(LOG_SERVER)
            << "Failed to disconnect " << security << " query worker.";
        return false;
    }

    // Don't log stop success.
    return true;
}

// Utilities.
//-----------------------------------------------------------------------------

////void query_worker::receive(zmq::socket& socket)
////{
////    incoming request;
////
////    if (!request.receive(socket))
////    {
////        log::warning(LOG_SERVER)
////            << "Malformed query from "
////            << encode_base16(request.address);
////        return;
////    }
////
////    if (log_)
////    {
////        log::info(LOG_SERVER)
////            << "Query " << request.command << " from "
////            << encode_base16(request.address);
////    }
////
////    // Locate the request handler for this command.
////    const auto handler = handlers_.find(request.command);
////
////    if (handler == handlers_.end())
////    {
////        log::warning(LOG_SERVER)
////            << "Invalid query from "
////            << encode_base16(request.address);
////        return;
////    }
////
////    // TODO: wrong endpoint.
////    // Execute the request if a handler exists and forward result to queue.
////    handler->second(request,
////        std::bind(&query_worker::send,
////            this, _1, std::ref(endpoint_)));
////}

////void query_worker::send(zmq::socket& socket, outgoing& response)
////{
////    if (response.send(socket))
////        return;
////
////    log::warning(LOG_SERVER)
////        << "Failed to send query response on " << endpoint_;
////}

// Query Interface.
// ----------------------------------------------------------------------------

// Class and method names must match protocol expectations (do not change).
#define ATTACH(class_name, method_name, instance) \
    attach(#class_name "." #method_name, \
        std::bind(&bc::server::class_name::method_name, \
            std::ref(instance), _1, _2));

void query_worker::attach(const std::string& command,
    command_handler handler)
{
    handlers_[command] = handler;
}

// Class and method names must match protocol expectations (do not change).
void query_worker::attach_interface()
{
    ////// TODO: add total_connections to client.
    ////ATTACH(protocol, total_connections, node_);
    ////ATTACH(protocol, broadcast_transaction, node_);
    ////ATTACH(transaction_pool, validate, node_);
    ////ATTACH(transaction_pool, fetch_transaction, node_);

    ////// TODO: add fetch_spend to client.
    ////// TODO: add fetch_block_transaction_hashes to client.
    ////ATTACH(blockchain, fetch_spend, node_);
    ////ATTACH(blockchain, fetch_transaction, node_);
    ////ATTACH(blockchain, fetch_last_height, node_);
    ////ATTACH(blockchain, fetch_block_header, node_);
    ////ATTACH(blockchain, fetch_block_height, node_);
    ////ATTACH(blockchain, fetch_transaction_index, node_);
    ////ATTACH(blockchain, fetch_stealth, node_);
    ////ATTACH(blockchain, fetch_history, node_);
    ////ATTACH(blockchain, fetch_block_transaction_hashes, node_);

    ////// address.fetch_history was present in v1 (obelisk) and v2 (server).
    ////// address.fetch_history was called by client v1 (sx) and v2 (bx).
    ////////ATTACH(endpoint, address, fetch_history, node_);
    ////ATTACH(address, fetch_history2, node_);

    ////// TODO: add renew to client.
    ////ATTACH(address, renew, address_notifier_);
    ////ATTACH(address, subscribe, address_notifier_);
}

#undef ATTACH

} // namespace server
} // namespace libbitcoin
