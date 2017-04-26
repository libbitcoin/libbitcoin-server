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
#include <bitcoin/server/services/transaction_service.hpp>

#include <functional>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::message;
using namespace bc::protocol;
using role = zmq::socket::role;

static const auto domain = "transaction";
static const auto public_worker = "inproc://public_tx";
static const auto secure_worker = "inproc://secure_tx";

transaction_service::transaction_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    security_(secure ? "secure" : "public"),
    settings_(node.server_settings()),
    external_(node.protocol_settings()),
    internal_(external_.send_high_water, external_.receive_high_water),
    service_(settings_.transaction_endpoint(secure)),
    worker_(secure ? secure_worker : public_worker),
    authenticator_(authenticator),
    node_(node),

    // Pick a random sequence counter start, will wrap around at overflow.
    sequence_(static_cast<uint16_t>(pseudo_random(0, max_uint16)))
{
}

// There is no unsubscribe so this class shouldn't be restarted.
bool transaction_service::start()
{
    // Subscribe to transaction pool acceptances.
    node_.subscribe_transaction(
        std::bind(&transaction_service::handle_transaction,
            this, _1, _2));

    return zmq::worker::start();
}

// Implement worker as extended pub-sub.
// The publisher drops messages for lost peers (clients) and high water.
void transaction_service::work()
{
    zmq::socket xpub(authenticator_, role::extended_publisher, external_);
    zmq::socket xsub(authenticator_, role::extended_subscriber, internal_);

    // Bind sockets to the service and worker endpoints.
    if (!started(bind(xpub, xsub)))
        return;

    // TODO: tap in to failure conditions, such as high water.
    // BUGBUG: stop is insufficient to stop the worker, because of relay().
    // Relay messages between subscriber and publisher (blocks on context).
    relay(xpub, xsub);

    // Unbind the sockets and exit this thread.
    finished(unbind(xpub, xsub));
}

// Bind/Unbind.
//-----------------------------------------------------------------------------

bool transaction_service::bind(zmq::socket& xpub, zmq::socket& xsub)
{
    if (!authenticator_.apply(xpub, domain, secure_))
        return false;

    auto ec = xpub.bind(service_);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " transaction service to "
            << service_ << " : " << ec.message();
        return false;
    }

    ec = xsub.bind(worker_);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " transaction workers to "
            << worker_ << " : " << ec.message();
        return false;
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " transaction service to " << service_;
    return true;
}

bool transaction_service::unbind(zmq::socket& xpub, zmq::socket& xsub)
{
    // Stop both even if one fails.
    const auto service_stop = xpub.stop();
    const auto worker_stop = xsub.stop();

    if (!service_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " transaction service.";

    if (!worker_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " transaction workers.";

    // Don't log stop success.
    return service_stop && worker_stop;
}

// Publish (integral worker).
// ----------------------------------------------------------------------------

bool transaction_service::handle_transaction(const code& ec,
    transaction_const_ptr tx)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling new transaction: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    // Nothing to do here, a channel is stopping.
    if (!tx)
        return true;

    // Do not announce txs to clients if too far behind.
    if (node_.chain().is_stale())
        return true;

    publish_transaction(tx);
    return true;
}

// [ tx... ]
void transaction_service::publish_transaction(transaction_const_ptr tx)
{
    if (stopped())
        return;

    zmq::socket publisher(authenticator_, role::publisher, internal_);

    // Subscriptions are off the pub-sub thread so this must connect back.
    // This could be optimized by caching the socket as thread static.
    auto ec = publisher.connect(worker_);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failed to connect " << security_ << " transaction worker: "
            << ec.message();
        return;
    }

    if (stopped())
        return;

    // [ sequence:2 ]
    // [ tx:... ]
    zmq::message broadcast;
    broadcast.enqueue_little_endian(++sequence_);
    broadcast.enqueue(tx->to_data(bc::message::version::level::canonical));

    ec = publisher.send(broadcast);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failed to publish " << security_ << " transaction ["
            << encode_hash(tx->hash()) << "] " << ec.message();
        return;
    }

    // This isn't actually a request, should probably update settings.
    LOG_VERBOSE(LOG_SERVER)
        << "Published " << security_ << " transaction ["
        << encode_hash(tx->hash()) << "] (" << sequence_ << ").";
}

} // namespace server
} // namespace libbitcoin
