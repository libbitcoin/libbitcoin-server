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

#include <cstdint>
#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messaging/incoming_message.hpp>
#include <bitcoin/server/messaging/request_worker.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class BCS_API server_node
  : public node::p2p_node
{
public:
    typedef std::function<void(uint32_t, const chain::block&)>
        block_notify_callback;
    typedef std::function<void (const chain::transaction&)>
        transaction_notify_callback;

    static const configuration defaults;

    server_node(const configuration& configuration=defaults);

    virtual void start(const settings& settings);
    virtual void subscribe_blocks(block_notify_callback notify_block);
    virtual void subscribe_transactions(transaction_notify_callback notify_tx);

private:
    typedef std::vector<block_notify_callback> block_notify_list;
    typedef std::vector<transaction_notify_callback> transaction_notify_list;

    bool handle_tx_validated(const code& ec, const chain::transaction& tx,
        const hash_digest& hash, const index_list& unconfirmed);

    bool handle_new_blocks(const code& ec, uint64_t fork_point,
        const blockchain::block_chain::list& new_blocks,
        const blockchain::block_chain::list& replaced_blocks);

    block_notify_list block_sunscriptions_;
    transaction_notify_list tx_subscriptions_;
    size_t last_checkpoint_height_;
    const configuration configuration_;
};

} // namespace server
} // namespace libbitcoin

#endif
