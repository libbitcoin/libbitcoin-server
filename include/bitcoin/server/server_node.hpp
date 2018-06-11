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
#ifndef LIBBITCOIN_SERVER_SERVER_NODE_HPP
#define LIBBITCOIN_SERVER_SERVER_NODE_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/node.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/messages/subscription.hpp>
#include <bitcoin/server/services/block_service.hpp>
#include <bitcoin/server/services/heartbeat_service.hpp>
#include <bitcoin/server/services/query_service.hpp>
#include <bitcoin/server/services/transaction_service.hpp>
#include <bitcoin/server/web/block_socket.hpp>
#include <bitcoin/server/web/heartbeat_socket.hpp>
#include <bitcoin/server/web/query_socket.hpp>
#include <bitcoin/server/web/transaction_socket.hpp>
#include <bitcoin/server/workers/authenticator.hpp>
#include <bitcoin/server/workers/notification_worker.hpp>

namespace libbitcoin {
namespace server {

class notification_worker;

class BCS_API server_node
  : public node::full_node
{
public:
    typedef std::shared_ptr<server_node> ptr;

    /// Construct a server node.
    server_node(const configuration& configuration);

    /// Ensure all threads are coalesced.
    virtual ~server_node();

    // Properties.
    // ----------------------------------------------------------------------------

    /// Server configuration settings.
    virtual const bc::protocol::settings& protocol_settings() const;

    /// Server configuration settings.
    virtual const bc::server::settings& server_settings() const;

    // Run sequence.
    // ------------------------------------------------------------------------

    /// Synchronize the blockchain and then begin long running sessions,
    /// call from start result handler. Call base method to skip sync.
    virtual void run(result_handler handler) override;

    // Shutdown.
    // ------------------------------------------------------------------------

    /// Idempotent call to signal work stop, start may be reinvoked after.
    /// Returns the result of file save operation.
    virtual bool stop() override;

    /// Blocking call to coalesce all work and then terminate all threads.
    /// Call from thread that constructed this class, or don't call at all.
    /// This calls stop, and start may be reinvoked after calling this.
    virtual bool close() override;

    // Notification.
    // ------------------------------------------------------------------------

    virtual code subscribe_address(const message& request,
        short_hash&& address_hash, bool unsubscribe);

    virtual code subscribe_stealth(const message& request,
        binary&& prefix_filter, bool unsubscribe);

private:
    void handle_running(const code& ec, result_handler handler);

    bool start_services();
    bool start_authenticator();
    bool start_query_services();
    bool start_heartbeat_services();
    bool start_block_services();
    bool start_transaction_services();
    bool start_query_workers(bool secure);
    bool start_notification_workers(bool secure);

    const configuration& configuration_;

    // These are thread safe.
    authenticator authenticator_;
    query_service secure_query_service_;
    query_service public_query_service_;

    // Zeromq services
    heartbeat_service secure_heartbeat_service_;
    heartbeat_service public_heartbeat_service_;
    block_service secure_block_service_;
    block_service public_block_service_;
    transaction_service secure_transaction_service_;
    transaction_service public_transaction_service_;
    notification_worker secure_notification_worker_;
    notification_worker public_notification_worker_;

    // Websocket services
    query_socket secure_query_websockets_;
    query_socket public_query_websockets_;
    heartbeat_socket secure_heartbeat_websockets_;
    heartbeat_socket public_heartbeat_websockets_;
    block_socket secure_block_websockets_;
    block_socket public_block_websockets_;
    transaction_socket secure_transaction_websockets_;
    transaction_socket public_transaction_websockets_;
};

} // namespace server
} // namespace libbitcoin

#endif
