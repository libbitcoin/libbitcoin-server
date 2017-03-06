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
#ifndef LIBBITCOIN_SERVER_NOTIFICATION_WORKER_HPP
#define LIBBITCOIN_SERVER_NOTIFICATION_WORKER_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/messages/route.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/address_key.hpp>

namespace libbitcoin {
namespace server {

class server_node;

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

    /// Subscribe to address and stealth prefix notifications.
    virtual code subscribe_address(const route& reply_to, uint32_t id,
        const binary& prefix_filter, bool unsubscribe);

protected:
    typedef bc::protocol::zmq::socket socket;

    virtual bool connect(socket& router);
    virtual bool disconnect(socket& router);

    // Implement the service.
    virtual void work() override;

private:
    typedef std::shared_ptr<uint16_t> sequence_ptr;
    typedef notifier<address_key, const code&, const binary&, uint32_t,
        const hash_digest&, transaction_const_ptr> address_subscriber;

    // Remove expired subscriptions.
    void purge();
    int32_t purge_interval_milliseconds() const;

    bool handle_reorganization(const code& ec, size_t fork_height,
        block_const_ptr_list_const_ptr new_blocks,
        block_const_ptr_list_const_ptr old_blocks);
    bool handle_transaction_pool(const code& ec, transaction_const_ptr tx);

    void notify_block(uint32_t height, block_const_ptr block);
    void notify_transaction(uint32_t height, const hash_digest& block_hash,
        transaction_const_ptr tx);

    void notify_address(const binary& field, uint32_t height,
        const hash_digest& block_hash, transaction_const_ptr tx);

    // Send a notification to the subscriber.
    void send(const route& reply_to, const std::string& command,
        uint32_t id, const data_chunk& payload);

    bool handle_address(const code& ec, const binary& field, uint32_t height,
        const hash_digest& block_hash, transaction_const_ptr tx,
        const route& reply_to, uint32_t id, const binary& prefix_filter,
        sequence_ptr sequence);

    const bool secure_;
    const server::settings& settings_;

    // These are thread safe.
    server_node& node_;
    bc::protocol::zmq::authenticator& authenticator_;
    address_subscriber::ptr address_subscriber_;
};

} // namespace server
} // namespace libbitcoin

#endif
