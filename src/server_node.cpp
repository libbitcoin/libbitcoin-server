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
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/interface/address.hpp>
#include <bitcoin/server/interface/blockchain.hpp>
#include <bitcoin/server/interface/protocol.hpp>
#include <bitcoin/server/interface/transaction_pool.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::node;
using namespace bc::protocol;
using namespace bc::wallet;

server_node::server_node(const configuration& configuration)
  : p2p_node(configuration),
    configuration_(configuration),
    last_checkpoint_height_(configuration.last_checkpoint_height())
{
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

    // The handler invoked on a new thread.
    p2p_node::start(
        std::bind(&server_node::handle_running,
            this, _1, handler));
}

#define ENDPOINT(endpoint, authenticator) \
    std::make_shared<endpoint>(authenticator, this)

void server_node::handle_running(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Declare services.
    authenticator_ = std::make_shared<curve_authenticator>(this);
    ////address_notifier_ = std::make_shared<address_notifier>(this);
    ////query_endpoint_ = ENDPOINT(query_endpoint, authenticator_);
    ////block_endpoint_ = ENDPOINT(block_endpoint, authenticator_);
    ////heartbeat_endpoint_ = ENDPOINT(heartbeat_endpoint, authenticator_);
    ////transaction_endpoint_ = ENDPOINT(transaction_endpoint, authenticator_);

    // Start authenticator monitor and services, these log internally.
    if (!authenticator_->start())
        ////!query_endpoint_->start() || !heartbeat_endpoint_->start() ||
        ////!block_endpoint_->start() || !transaction_endpoint_->start())
    {
        handler(error::operation_failed);
        return;
    }

    ////// Attach service handlers.
    ////if (server_settings().query_endpoint_enabled)
    ////{
    ////    attach_query_api();

    ////    if (server_settings().subscription_limit > 0)
    ////        attach_subscription_api();
    ////}

    // Subscribe to blockchain reorganizations.
    subscribe_blockchain(
        std::bind(&server_node::handle_new_blocks,
            this, _1, _2, _3, _4));

    // Subscribe to transaction pool acceptances.
    subscribe_transaction_pool(
        std::bind(&server_node::handle_new_transaction,
            this, _1, _2, _3));

    // This is the end of the derived run sequence.
    handler(error::success);
}

#undef ENDPOINT

// Stop sequence.
// ----------------------------------------------------------------------------

void server_node::stop(result_handler handler)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    transaction_mutex_.lock();
    transaction_subscriptions_.clear();
    transaction_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    block_mutex_.lock();
    block_subscriptions_.clear();
    block_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // Stop all work so that the threadpool can join.
    ////query_endpoint_->stop();
    ////heartbeat_endpoint_->stop();
    authenticator_->stop();

    // This is invoked on a new thread.
    // This is the end of the derived stop sequence.
    p2p_node::stop(handler);
}

// Close sequence.
// ----------------------------------------------------------------------------

// This allows for shutdown based on destruct without need to call stop.
server_node::~server_node()
{
    server_node::close();
}

// This must be called from the thread that constructed this class (see join).
// Okay to ignore code as we are in the destructor, use stop if code is needed.
void server_node::close()
{
    // This must block until handle_closing completes.
    std::promise<code> wait;

    server_node::stop(
        std::bind(&server_node::handle_closing,
            this, _1, std::ref(wait)));

    wait.get_future();
    p2p_node::close();
}

void server_node::handle_closing(const code& ec, std::promise<code>& wait)
{
    // This is the end of the derived close sequence.
    wait.set_value(ec);
}

// Properties.
// ----------------------------------------------------------------------------

const settings& server_node::server_settings() const
{
    return configuration_.server;
}

// Subscriptions.
// ----------------------------------------------------------------------------

// This serves both address subscription and the block publisher.
void server_node::subscribe_blocks(block_notify_callback notify_block)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(block_mutex_);

    if (!stopped())
        block_subscriptions_.push_back(notify_block);
    ///////////////////////////////////////////////////////////////////////////
}

// This serves both address subscription and the tx publisher.
void server_node::subscribe_transactions(transaction_notify_callback notify_tx)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(transaction_mutex_);

    if (!stopped())
        transaction_subscriptions_.push_back(notify_tx);
    ///////////////////////////////////////////////////////////////////////////
}

// Notifications.
// ----------------------------------------------------------------------------

bool server_node::handle_new_transaction(const code& ec,
    const point::indexes& unconfirmed, const transaction& tx)
{
    if (stopped() || ec == bc::error::service_stopped)
        return false;

    if (ec)
    {
        log::error(LOG_SERVICE)
            << "Failure handling new tx: " << ec.message();
        return false;
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    transaction_mutex_.lock_shared();
    const auto transaction_subscriptions = transaction_subscriptions_;
    transaction_mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    // Fire server protocol tx subscription notifications.
    for (const auto notify : transaction_subscriptions)
        notify(tx);

    return true;
}

bool server_node::handle_new_blocks(const code& ec, uint64_t fork_point,
    const block::ptr_list& new_blocks, const block::ptr_list& replaced_blocks)
{
    if (stopped() || ec == bc::error::service_stopped)
        return false;

    if (fork_point < last_checkpoint_height_)
        return false;

    if (ec)
    {
        log::error(LOG_SERVICE)
            << "Failure handling new blocks: " << ec.message();
        return false;
    }

    BITCOIN_ASSERT(fork_point < max_uint32 - new_blocks.size());
    auto height = static_cast<uint32_t>(fork_point);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    block_mutex_.lock_shared();
    const auto block_subscriptions = block_subscriptions_;
    block_mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    // Fire server protocol block subscription notifications.
    for (auto new_block : new_blocks)
        for (const auto notify : block_subscriptions)
            notify(++height, new_block);

    return true;
}

// ----------------------------------------------------------------------------
// Server API bindings.

// Class and method names must match protocol expectations (do not change).
#define ATTACH(endpoint, class_name, method_name, instance) \
    endpoint->attach(#class_name "." #method_name, \
        std::bind(&bc::server::class_name::method_name, \
            instance, _1, _2));

// Class and method names must match protocol expectations (do not change).
void server_node::attach_subscription_api()
{
    // TODO: add renew to client.
    ATTACH(query_endpoint_, address, renew, address_notifier_);
    ATTACH(query_endpoint_, address, subscribe, address_notifier_);
}

// Class and method names must match protocol expectations (do not change).
void server_node::attach_query_api()
{
    // TODO: add total_connections to client.
    ATTACH(query_endpoint_, protocol, total_connections, this);
    ATTACH(query_endpoint_, protocol, broadcast_transaction, this);
    ATTACH(query_endpoint_, transaction_pool, validate, this);
    ATTACH(query_endpoint_, transaction_pool, fetch_transaction, this);

    // TODO: add fetch_spend to client.
    // TODO: add fetch_block_transaction_hashes to client.
    ATTACH(query_endpoint_, blockchain, fetch_spend, this);
    ATTACH(query_endpoint_, blockchain, fetch_block_transaction_hashes, this);
    ATTACH(query_endpoint_, blockchain, fetch_transaction, this);
    ATTACH(query_endpoint_, blockchain, fetch_last_height, this);
    ATTACH(query_endpoint_, blockchain, fetch_block_header, this);
    ATTACH(query_endpoint_, blockchain, fetch_block_height, this);
    ATTACH(query_endpoint_, blockchain, fetch_transaction_index, this);
    ATTACH(query_endpoint_, blockchain, fetch_stealth, this);
    ATTACH(query_endpoint_, blockchain, fetch_history, this);

    // address.fetch_history was present in v1 (obelisk) and v2 (server).
    // address.fetch_history was called by client v1 (sx) and v2 (bx).
    ////ATTACH(query_endpoint_, address, fetch_history, this);
    ATTACH(query_endpoint_, address, fetch_history2, this);
}

#undef ATTACH

} // namespace server
} // namespace libbitcoin
