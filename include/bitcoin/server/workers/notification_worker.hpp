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
#ifndef LIBBITCOIN_SERVER_NOTIFICATION_WORKER_HPP
#define LIBBITCOIN_SERVER_NOTIFICATION_WORKER_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

////struct subscription_locator
////{
////    send_handler handler;
////    data_chunk address1;
////    data_chunk address2;
////    bool delimited;
////};
////
////struct subscription_record
////{
////    binary prefix;
////    chain::subscribe_type type;
////    boost::posix_time::ptime expiry_time;
////    subscription_locator locator;
////};

// This class is thread safe.
// Provide address and stealth notifications to the query service.
class BCS_API notification_worker
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<notification_worker> ptr;

    /// Construct an address worker.
    notification_worker(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// Start the worker.
    bool start() override;

    /// Stop the worker.
    bool stop() override;

    /////// Subscribe to address and stealth prefix notifications.
    ////virtual void subscribe_address(route& reply_to, binary& prefix_filter,
    ////    chain::subscribe_type& type);

    /////// Subscribe to address and stealth prefix notifications.
    ////virtual void subscribe_radar(route& reply_to, hash_digest& tx_hash);

protected:
    typedef bc::protocol::zmq::socket socket;

    virtual bool connect(socket& router);
    virtual bool disconnect(socket& router);

    // Implement the service.
    virtual void work();

private:
    typedef chain::block::ptr_list block_list;
    typedef chain::point::indexes index_list;

    typedef resubscriber<const wallet::payment_address&, uint32_t,
        const hash_digest&, const chain::transaction&> address_subscriber;
    typedef resubscriber<uint32_t, const hash_digest&, const hash_digest&>
        inventory_subscriber;
    typedef resubscriber<uint32_t, uint32_t, const hash_digest&,
        const chain::transaction&> stealth_subscriber;

    bool handle_blockchain_reorganization(const code& ec, uint64_t fork_point,
        const block_list& new_blocks, const block_list&);
    bool handle_transaction_pool(const code& ec, const index_list&,
        const chain::transaction& tx);
    bool handle_inventory(const code& ec,
        const message::inventory::ptr packet);

    void notify_blocks(uint32_t fork_point, const block_list& blocks);
    void notify_block(socket& peer, uint32_t height,
        const chain::block::ptr block);
    void notify_transaction(uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);
    void notify_address(const wallet::payment_address& address,
        uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);
    void notify_stealth(uint32_t prefix, uint32_t height,
        const hash_digest& block_hash, const chain::transaction& tx);
    void notify_inventory(uint32_t height, const hash_digest& block_hash,
        const hash_digest& tx_hash);

    ////static boost::posix_time::ptime now();

    ////void scan(uint32_t height, const hash_digest& block_hash,
    ////    const chain::transaction& tx);

    ////void post_updates(const wallet::payment_address& address,
    ////    uint32_t height, const hash_digest& block_hash,
    ////    const chain::transaction& tx);
    ////void post_stealth_updates(uint32_t prefix, uint32_t height,
    ////    const hash_digest& block_hash, const chain::transaction& tx);

    size_t prune() { return 0; }
    ////code create(const incoming& request, send_handler handler);
    ////code update(const incoming& request, send_handler handler);
    ////bool deserialize(binary& address, chain::subscribe_type& type,
    ////    const data_chunk& data);

    const bool secure_;
    const server::settings& settings_;

    // These are thread safe.
    server_node& node_;
    bc::protocol::zmq::authenticator& authenticator_;
    address_subscriber::ptr address_subscriber_;
    inventory_subscriber::ptr inventory_subscriber_;
    stealth_subscriber::ptr stealth_subscriber_;
};

} // namespace server
} // namespace libbitcoin

#endif
