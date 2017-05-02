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
#include <bitcoin/server/workers/query_worker.hpp>

#include <functional>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interface/blockchain.hpp>
#include <bitcoin/server/interface/subscribe.hpp>
#include <bitcoin/server/interface/transaction_pool.hpp>
#include <bitcoin/server/interface/unsubscribe.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::protocol;
using role = zmq::socket::role;

query_worker::query_worker(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    security_(secure ? "secure" : "public"),
    settings_(node.server_settings()),
    external_(node.protocol_settings()),
    internal_(external_.send_high_water, external_.receive_high_water),
    worker_(query_service::worker_endpoint(secure)),
    authenticator_(authenticator),
    node_(node)
{
    // The same interface is attached to the secure and public interfaces.
    attach_interface();
}

// Implement worker as a dealer to the query service.
// v2 libbitcoin-client DEALER does not add delimiter frame.
// The dealer drops messages for lost peers (query service) and high water.
void query_worker::work()
{
    // Use a dealer for this synchronous response because notifications are
    // sent asynchronously to the same identity via the same dealer. Using a
    // router is okay but it adds an additional address to the envelope that
    // would have to be stripped by the notification dealer so this is simpler.
    zmq::socket dealer(authenticator_, role::dealer, internal_);

    // Connect socket to the service endpoint.
    if (!started(connect(dealer)))
        return;

    zmq::poller poller;
    poller.add(dealer);

    while (!poller.terminated() && !stopped())
    {
        if (poller.wait().contains(dealer.id()))
            query(dealer);
    }

    // Disconnect the socket and exit this thread.
    finished(disconnect(dealer));
}

// Connect/Disconnect.
//-----------------------------------------------------------------------------

bool query_worker::connect(zmq::socket& dealer)
{
    const auto ec = dealer.connect(worker_);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect " << security_ << " query worker to "
            << worker_ << " : " << ec.message();
        return false;
    }

    LOG_DEBUG(LOG_SERVER)
        << "Connected " << security_ << " query worker to " << worker_;
    return true;
}

bool query_worker::disconnect(zmq::socket& dealer)
{
    // Don't log stop success.
    if (dealer.stop())
        return true;

    LOG_ERROR(LOG_SERVER)
        << "Failed to disconnect " << security_ << " query worker.";
    return false;
}

// Query Execution.
// The dealer send blocks until the query service dealer is available.
//-----------------------------------------------------------------------------

// private/static
void query_worker::send(const message& response, zmq::socket& dealer)
{
    const auto ec = response.send(dealer);

    if (ec && ec != error::service_stopped)
        LOG_WARNING(LOG_SERVER)
            << "Failed to send query response to "
            << response.route().display() << " " << ec.message();
}

// Because the socket is a router we may simply drop invalid queries.
// As a single thread worker this router should not reach high water.
// If we implemented as a replier we would need to always provide a response.
void query_worker::query(zmq::socket& dealer)
{
    if (stopped())
        return;

    message request(secure_);
    const auto ec = request.receive(dealer);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_DEBUG(LOG_SERVER)
            << "Failed to receive query from " << request.route().display()
            << " " << ec.message();

        send(message(request, ec), dealer);
        return;
    }

    // Locate the request handler for this command.
    const auto handler = command_handlers_.find(request.command());

    if (handler == command_handlers_.end())
    {
        LOG_DEBUG(LOG_SERVER)
            << "Invalid query command from " << request.route().display();

        send(message(request, error::not_found), dealer);
        return;
    }

    LOG_VERBOSE(LOG_SERVER)
        << "Query " << request.command() << " from "
        << request.route().display();

    // The query executor is the delegate bound by the attach method.
    const auto& query_execute = handler->second;

    // Execute the request and send the result.
    // Example: address.renew(node_, request, sender);
    // Example: blockchain.fetch_history3(node_, request, sender);
    query_execute(request,
        std::bind(&query_worker::send,
            _1, std::ref(dealer)));
}

// Query Interface.
// ----------------------------------------------------------------------------

// Class and method names must match protocol expectations (do not change).
#define ATTACH(class_name, method_name, node) \
    attach(#class_name "." #method_name, \
        std::bind(&bc::server::class_name::method_name, \
            std::ref(node), _1, _2));

void query_worker::attach(const std::string& command,
    command_handler handler)
{
    command_handlers_[command] = handler;
}

//=============================================================================
// TODO: add to client and bx:
// blockchain.fetch_spend
// blockchain.fetch_block_height
// blockchain.fetch_block_transaction_hashes
// blockchain.fetch_stealth_transaction_hashes [document name change]
// subscribe.block (pub-sub)
// subscribe.transaction (pub-sub)
// subscribe.address
// subscribe.stealth
// unsubscribe.address
// unsubscribe.stealth
// TODO: add fetch_block (full)
//=============================================================================
// address.fetch_history is obsoleted in v3 (no unconfirmed tx indexing).
// address.renew is obsoleted in v3.
// address.subscribe is obsoleted in v3.
//-----------------------------------------------------------------------------
// blockchain.validate is new in v3 (blocks).
// blockchain.broadcast is new in v3 (blocks).
// blockchain.fetch_history is obsoleted in v3 (hash reversal).
// blockchain.fetch_history2 was new in v3.
// blockchain.fetch_history2 is obsoleted in v3.1 (version byte unused)
// blockchain.fetch_history3 is new in v3.1 (no version byte)
// blockchain.fetch_stealth is obsoleted in v3 (hash reversal).
// blockchain.fetch_stealth2 is new in v3.
// blockchain.fetch_stealth_transaction_hashes is new in v3 (safe version).
//-----------------------------------------------------------------------------
// transaction_pool.validate is obsoleted in v3 (unconfirmed outputs).
// transaction_pool.validate2 is new in v3.
// transaction_pool.broadcast is new in v3 (rename).
// transaction_pool.fetch_transaction is enhanced in v3 (adds confirmed txs).
//-----------------------------------------------------------------------------
// protocol.broadcast_transaction is obsoleted in v3 (renamed).
// protocol.total_connections is obsoleted in v3 (administrative).
//-----------------------------------------------------------------------------
// subscribe.address is new in v3, also call for renew.
// subscribe.stealth is new in v3, also call for renew.
//-----------------------------------------------------------------------------
// unsubscribe.address is new in v3 (there was never address.unsubscribe).
// unsubscribe.stealth is new in v3 (there was never stealth.unsubscribe).
//=============================================================================
// Interface class.method names must match protocol names.
void query_worker::attach_interface()
{
    // Subscription was not operational in version 3.0.
    ////ATTACH(address, renew, node_);                    // obsoleted (3.0)
    ////ATTACH(address, subscribe, node_);                // obsoleted (3.0)
    ////ATTACH(address, fetch_history, node_);            // obsoleted (3.0)

    ATTACH(subscribe, address, node_);                          // new (3.1)
    ATTACH(subscribe, stealth, node_);                          // new (3.1)
    ATTACH(unsubscribe, address, node_);                        // new (3.1)
    ATTACH(unsubscribe, stealth, node_);                        // new (3.1)

    ////ATTACH(blockchain, fetch_stealth, node_);               // obsoleted
    ////ATTACH(blockchain, fetch_history, node_);               // obsoleted
    ////ATTACH(blockchain, fetch_history2, node_);              // obsoleted
    ATTACH(blockchain, fetch_block_header, node_);              // original
    ATTACH(blockchain, fetch_block_height, node_);              // original
    ATTACH(blockchain, fetch_block_transaction_hashes, node_);  // original
    ATTACH(blockchain, fetch_last_height, node_);               // original
    ATTACH(blockchain, fetch_transaction, node_);               // original
    ATTACH(blockchain, fetch_transaction_index, node_);         // original
    ATTACH(blockchain, fetch_spend, node_);                     // original
    ATTACH(blockchain, fetch_history3, node_);                  // new (3.1)
    ATTACH(blockchain, fetch_stealth2, node_);                  // new (3.0)
    ATTACH(blockchain, fetch_stealth_transaction_hashes, node_);// new (3.0)
    ATTACH(blockchain, broadcast, node_);                       // new (3.0)
    ATTACH(blockchain, validate, node_);                        // new (3.0)

    ////ATTACH(transaction_pool, validate, node_);              // obsoleted
    ATTACH(transaction_pool, fetch_transaction, node_);         // enhanced
    ATTACH(transaction_pool, broadcast, node_);                 // new (3.0)
    ATTACH(transaction_pool, validate2, node_);                 // new (3.0)

    ////ATTACH(protocol, broadcast_transaction, node_);         // obsoleted
    ////ATTACH(protocol, total_connections, node_);             // obsoleted
}

#undef ATTACH

} // namespace server
} // namespace libbitcoin
