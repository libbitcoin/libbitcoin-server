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
#include <memory>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>

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

    server_node(const configuration& configuration);

    virtual const settings& server_settings() const;
    virtual void start(result_handler handler);
    virtual void subscribe_blocks(block_notify_callback notify_block);
    virtual void subscribe_transactions(transaction_notify_callback notify_tx);

private:
    typedef std::vector<block_notify_callback> block_notify_list;
    typedef std::vector<transaction_notify_callback> transaction_notify_list;

    void handle_node_start(const code& ec, result_handler handler);
    bool handle_tx_accepted(const code& ec, const index_list& unconfirmed,
        const chain::transaction& tx);
    bool handle_new_blocks(const code& ec, uint64_t fork_point,
        const chain::block::ptr_list& new_blocks,
        const chain::block::ptr_list& replaced_blocks);

    block_notify_list block_sunscriptions_;
    transaction_notify_list tx_subscriptions_;
    size_t last_checkpoint_height_;
    const configuration& configuration_;
};

} // namespace server
} // namespace libbitcoin

#endif
