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
#include <bitcoin/server/services/block_service.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::protocol;

static const auto domain = "block";
const config::endpoint block_service::public_worker("inproc://public_block");
const config::endpoint block_service::secure_worker("inproc://secure_block");

block_service::block_service(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : worker(priority(node.server_settings().priority)),
    secure_(secure),
    verbose_(node.network_settings().verbose),
    settings_(node.server_settings()),
    authenticator_(authenticator),
    node_(node),

    // Pick a random sequence counter start, will wrap around at overflow.
    sequence_(static_cast<uint16_t>(pseudo_random(0, max_uint16)))
{
}

// There is no unsubscribe so this class shouldn't be restarted.
bool block_service::start()
{
    // Subscribe to blockchain reorganizations.
    node_.subscribe_blockchain(
        std::bind(&block_service::handle_reorganization,
            this, _1, _2, _3, _4));

    return zmq::worker::start();
}

// Implement worker as extended pub-sub.
// The publisher drops messages for lost peers (clients) and high water.
void block_service::work()
{
    zmq::socket xpub(authenticator_, zmq::socket::role::extended_publisher);
    zmq::socket xsub(authenticator_, zmq::socket::role::extended_subscriber);

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

bool block_service::bind(zmq::socket& xpub, zmq::socket& xsub)
{
    const auto security = secure_ ? "secure" : "public";
    const auto& worker = secure_ ? secure_worker : public_worker;
    const auto& service = secure_ ? settings_.secure_block_endpoint :
        settings_.public_block_endpoint;

    if (!authenticator_.apply(xpub, domain, secure_))
        return false;

    auto ec = xpub.bind(service);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security << " block service to "
            << service << " : " << ec.message();
        return false;
    }

    ec = xsub.bind(worker);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security << " block workers to "
            << worker << " : " << ec.message();
        return false;
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security << " block service to " << service;
    return true;
}

bool block_service::unbind(zmq::socket& xpub, zmq::socket& xsub)
{
    // Stop both even if one fails.
    const auto service_stop = xpub.stop();
    const auto worker_stop = xsub.stop();
    const auto security = secure_ ? "secure" : "public";

    if (!service_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security << " block service.";

    if (!worker_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security << " block workers.";

    // Don't log stop success.
    return service_stop && worker_stop;
}

// Publish (integral worker).
// ----------------------------------------------------------------------------

bool block_service::handle_reorganization(const code& ec, size_t fork_height,
    block_const_ptr_list_const_ptr new_blocks, block_const_ptr_list_const_ptr)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling new block: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    // Nothing to do here.
    if (!new_blocks || new_blocks->empty())
        return true;

    // Blockchain height is 64 bit but obelisk protocol is 32 bit.
    publish_blocks(safe_unsigned<uint32_t>(fork_height), new_blocks);
    return true;
}

void block_service::publish_blocks(uint32_t fork_height,
    block_const_ptr_list_const_ptr blocks)
{
    if (stopped())
        return;

    const auto security = secure_ ? "secure" : "public";
    const auto& endpoint = secure_ ? block_service::secure_worker :
        block_service::public_worker;

    // Subscriptions are off the pub-sub thread so this must connect back.
    // This could be optimized by caching the socket as thread static.
    zmq::socket publisher(authenticator_, zmq::socket::role::publisher);
    const auto ec = publisher.connect(endpoint);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failed to connect " << security << " block worker: "
            << ec.message();
        return;
    }

    for (const auto block: *blocks)
        publish_block(publisher, fork_height++, block);
}

// [ height:4 ]
// [ block ]
// The payload for block publication is delimited within the zeromq message.
// This is required for compatability and inconsistent with query payloads.
void block_service::publish_block(zmq::socket& publisher, size_t height,
    block_const_ptr block)
{
    if (stopped())
        return;

    const auto security = secure_ ? "secure" : "public";

    // [ sequence:2 ]
    // [ height:4 ]
    // [ block:... ]
    zmq::message broadcast;
    broadcast.enqueue_little_endian(++sequence_);
    broadcast.enqueue_little_endian(static_cast<uint32_t>(height));
    broadcast.enqueue(block->to_data(bc::message::version::level::canonical));

    const auto ec = publisher.send(broadcast);

    if (ec == error::service_stopped)
        return;

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failed to publish " << security << " bloc ["
            << encode_hash(block->hash()) << "] " << ec.message();
        return;
    }

    // This isn't actually a request, should probably update settings.
    if (verbose_)
        LOG_DEBUG(LOG_SERVER)
            << "Published " << security << " block ["
            << encode_hash(block->hash()) << "] (" << sequence_ << ").";
}

} // namespace server
} // namespace libbitcoin
