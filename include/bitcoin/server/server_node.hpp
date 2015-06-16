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

#include <bitcoin/node.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

class BCS_API server_node
  : public node::full_node
{
public:
    typedef std::function<void (size_t, const chain::block&)>
        block_notify_callback;

    typedef std::function<void (const chain::transaction&)>
        transaction_notify_callback;

    server_node(settings_type& config);

    bool start(settings_type& config);

    virtual void subscribe_blocks(block_notify_callback notify_block);
    virtual void subscribe_transactions(transaction_notify_callback notify_tx);

    static void fullnode_fetch_history(server_node& node,
        const incoming_message& request, queue_send_callback queue_send);

protected:
    // Result of store operation in transaction pool.
    virtual void new_unconfirm_valid_tx(const std::error_code& ec,
        const chain::index_list& unconfirmed, const chain::transaction& tx);

    virtual void broadcast_new_blocks(const std::error_code& ec,
        uint32_t fork_point,
        const blockchain::blockchain::block_list& new_blocks,
        const blockchain::blockchain::block_list& replaced_blocks);

private:

    typedef std::vector<block_notify_callback> block_notify_list;

    typedef std::vector<transaction_notify_callback> transaction_notify_list;

    // Subscriptions for server API.
    block_notify_list block_sunscriptions_;
    transaction_notify_list tx_subscriptions_;

    // Logs.
    bc::ofstream outfile_;
    bc::ofstream errfile_;

    // Services.
    bc::threadpool network_threads_;
    bc::threadpool database_threads_;
    bc::threadpool memory_threads_;
    bc::network::hosts host_pool_;
    bc::network::handshake handshake_;
    bc::network::network network_;
    bc::network::protocol protocol_;
    bc::blockchain::blockchain_impl blockchain_;
    bc::blockchain::transaction_pool tx_pool_;
    bc::node::indexer tx_indexer_;
    bc::node::poller poller_;
    bc::node::session session_;
    bc::node::responder responder_;

    // Timer.
    boost::asio::deadline_timer retry_start_timer_;
};

} // namespace server
} // namespace libbitcoin

#endif
