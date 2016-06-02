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

query_worker::query_worker(zmq::context& context, server_node& node)
  : worker(node.thread_pool()),
    log_(node.server_settings().log_requests),
    enabled_(node.server_settings().query_endpoints_enabled),
    address_notifier_(node),
    context_(context)
{
}

bool query_worker::start()
{
    if (!enabled_)
        return true;

    if (!zmq::worker::start())
    {
        log::error(LOG_SERVER)
            << "Failed to bind inproc query worker to "
            << query_endpoint::workers;
        return false;
    }

    log::debug(LOG_SERVER)
        << "Bound inproc query worker to " << query_endpoint::workers;
    return true;
}

bool query_worker::stop()
{
    if (!zmq::worker::stop())
    {
        log::error(LOG_SERVER)
            << "Failed to unbind inproc query worker from "
            << query_endpoint::workers;
        return false;
    }

    log::debug(LOG_SERVER)
        << "Unbound inproc query worker from " << query_endpoint::workers;
    return true;
}

// Implement worker as a replier.
// A replier requires strict receive-send ordering.
void query_worker::work()
{
    zmq::socket replier(context_, zmq::socket::role::replier);

    if (!started(replier.connect(query_endpoint::workers)))
        return;

    zmq::poller poller;
    poller.add(replier);

    while (!poller.terminated() && !stopped())
    {
        if (!poller.wait().contains(replier.id()))
            continue;

        // In order to handle asynchronous reply we need a router here.

        // NOTE: v2 libbitcoin-client DEALER does not add delimiter frame.
        zmq::message request;
        auto result = request.receive(replier);

        // Process request->data.

        ////outgoing response(request, error::success);
        ////result = response.send(replier);
    }

    // Stop the socket and exit this thread.
    finished(replier.stop());
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

////void query_worker::send(outgoing& response, const config::endpoint& query_worker)
////{
////    // TODO: make members of query worker.
////    // TODO: in stop we have no way to detect completion of all callback work.
////    // TODO: this can only be terminated by thread join. so callback worker
////    // stop must be non-blocking and the worker and context must be kept in
////    // scope until thread join. Both stopped_ and context stop will signal
////    // callback worker stop. Socket destruct won't happen until thread join
////    // so context must be kept in scope until after node stop.
////    zmq::context context_;
////    boost::thread_specific_ptr<zmq::socket> socket_ptr_();
////
////    if (!socket_ptr_.get())
////    {
////        static const auto role = zmq::socket::role::replier;
////        socket_ptr_.reset(new zmq::socket(context_, role));
////        socket_ptr_->bind(query_worker);
////    }
////    
////    if (response.send(*socket_ptr_))
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
