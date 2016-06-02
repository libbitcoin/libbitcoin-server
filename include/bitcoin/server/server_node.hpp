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
#ifndef LIBBITCOIN_SERVER_SERVER_NODE_HPP
#define LIBBITCOIN_SERVER_SERVER_NODE_HPP

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/services/block_service.hpp>
#include <bitcoin/server/services/heart_service.hpp>
#include <bitcoin/server/services/query_service.hpp>
#include <bitcoin/server/services/trans_service.hpp>
#include <bitcoin/server/utility/address_notifier.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>
#include <bitcoin/server/workers/query_worker.hpp>

namespace libbitcoin {
namespace server {

class BCS_API server_node
  : public node::p2p_node
{
public:
    typedef std::shared_ptr<server_node> ptr;
    typedef std::function<void(uint32_t, const chain::block::ptr)>
        block_notify_callback;
    typedef std::function<void (const chain::transaction&)>
        transaction_notify_callback;

    /// Construct a server node.
    server_node(const configuration& configuration);

    /// Ensure all threads are coalesced.
    virtual ~server_node();

    // Start/Run/Stop/Close sequences.
    // ------------------------------------------------------------------------

    /// Synchronize the blockchain and then begin long running sessions,
    /// call from start result handler. Call base method to skip sync.
    virtual void run(result_handler handler) override;

    /// Non-blocking call to coalesce all work, start may be reinvoked after.
    /// Handler returns the result of file save operations.
    virtual void stop(result_handler handler) override;

    /// Blocking call to coalesce all work and then terminate all threads.
    /// Call from thread that constructed this class, or don't call at all.
    /// This calls stop, and start may be reinvoked after calling this.
    virtual void close() override;

    // Properties.
    // ----------------------------------------------------------------------------

    /// Server configuration settings.
    virtual const settings& server_settings() const;

    // Subscriptions.
    // ----------------------------------------------------------------------------

    /// Subscribe to block announcements and reorgs.
    virtual void subscribe_blocks(block_notify_callback notify_block);

    /// Subscribe to new memory pool transactions.
    virtual void subscribe_transactions(transaction_notify_callback notify_tx);

private:
    typedef std::vector<block_notify_callback> block_notify_list;
    typedef std::vector<transaction_notify_callback> transaction_notify_list;

    bool handle_new_transaction(const code& ec,
        const chain::point::indexes& unconfirmed,
        const chain::transaction& tx);
    bool handle_new_blocks(const code& ec, uint64_t fork_point,
        const chain::block::ptr_list& new_blocks,
        const chain::block::ptr_list& replaced_blocks);

    void handle_running(const code& ec, result_handler handler);
    void handle_closing(const code& ec, std::promise<code>& wait);

    const configuration& configuration_;
    const size_t last_checkpoint_height_;

    // These are thread safe.
    curve_authenticator authenticator_;
    query_worker query_worker_;
    query_service secure_query_endpoint_;
    query_service public_query_endpoint_;

    // This is protected by block mutex.
    block_notify_list block_subscriptions_;
    mutable upgrade_mutex block_mutex_;

    // This is protected by transaction mutex.
    transaction_notify_list transaction_subscriptions_;
    mutable upgrade_mutex transaction_mutex_;
};

} // namespace server
} // namespace libbitcoin

#endif
