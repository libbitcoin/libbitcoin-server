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
////#include <bitcoin/server/services/address_worker.hpp>
#include <bitcoin/server/utility/authenticator.hpp>

namespace libbitcoin {
namespace server {

class BCS_API server_node
  : public node::p2p_node
{
public:
    typedef std::shared_ptr<server_node> ptr;

    /// Construct a server node.
    server_node(const configuration& configuration);

    /// Ensure all threads are coalesced.
    virtual ~server_node();

    // Start/Run sequences.
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

    // Properties.
    // ----------------------------------------------------------------------------

    /// Server configuration settings.
    virtual const settings& server_settings() const;

private:
    void handle_running(const code& ec, result_handler handler);

    bool start_query_services();
    bool start_heart_services();
    bool start_block_services();
    bool start_trans_services();
    bool start_query_workers(bool secure);

    const configuration& configuration_;

    // These are thread safe.
    authenticator authenticator_;
    query_service secure_query_service_;
    query_service public_query_service_;
    heart_service secure_heart_service_;
    heart_service public_heart_service_;
    block_service secure_block_service_;
    block_service public_block_service_;
    trans_service secure_trans_service_;
    trans_service public_trans_service_;
};

} // namespace server
} // namespace libbitcoin

#endif
