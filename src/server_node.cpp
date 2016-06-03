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
    last_checkpoint_height_(configuration.last_checkpoint_height()),
    authenticator_(*this),
    secure_query_service_(authenticator_, *this, true),
    public_query_service_(authenticator_, *this, false)
{
}

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

    // Signals close and blocks until all sockets are closed.
    authenticator_.stop();

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
void server_node::close()
{
    std::promise<code> wait;

    server_node::stop(
        std::bind(&server_node::handle_closing,
            this, _1, std::ref(wait)));

    // This blocks until handle_closing completes.
    wait.get_future().get();
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

    // Start query services and workers.
    start_query_services(handler);

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

// Services.
// ----------------------------------------------------------------------------

// The number of threads available in the thread pool must be sufficient
// to allocate the workers. This will wait forever on thread availability.
void server_node::allocate_query_workers(bool secure, result_handler handler)
{
    auto& server = *this;
    const auto& settings = configuration_.server;

    for (auto count = 0; count < settings.query_workers; ++count)
    {
        auto worker = std::make_shared<query_worker>(authenticator_,
            server, secure);

        if (!worker->start())
        {
            handler(error::operation_failed);
            return;
        }

        subscribe_stop([=](const code&) { worker->stop(); });
    }
}

void server_node::start_query_services(result_handler handler)
{
    const auto& settings = configuration_.server;

    if (!settings.query_service_enabled)
        return;

    if (settings.server_private_key)
    {
        if (!secure_query_service_.start())
        {
            handler(error::operation_failed);
            return;
        }

        allocate_query_workers(true, handler);
    }

    if (!settings.secure_only)
    {
        if (!public_query_service_.start())
        {
            handler(error::operation_failed);
            return;
        }

        allocate_query_workers(false, handler);
    }
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
        log::error(LOG_SERVER)
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
    for (const auto notify: transaction_subscriptions)
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
        log::error(LOG_SERVER)
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
    for (auto new_block: new_blocks)
        for (const auto notify: block_subscriptions)
            notify(++height, new_block);

    return true;
}

} // namespace server
} // namespace libbitcoin
