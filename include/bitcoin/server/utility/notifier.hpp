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
#ifndef LIBBITCOIN_SERVER_NOTIFICATIONS_HPP
#define LIBBITCOIN_SERVER_NOTIFICATIONS_HPP

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>

// This class is thread safe.
namespace libbitcoin {
namespace server {

/// Address notification subscription and renewal.
class BCS_API notifier
{
public:
    notifier();
    virtual void subscribe(const incoming& request, send_handler handler);
    virtual void renew(const incoming& request, send_handler handler);

private:
    // TODO: generalize addressing metadata.
    struct subscription_locator
    {
        send_handler handler;
        data_chunk address1;
        data_chunk address2;
        bool delimited;
    };

    // TODO: implement as callbacks to the subscriber.
    struct subscription_record
    {
        binary prefix;
        chain::subscribe_type type;
        boost::posix_time::ptime expiry_time;
        subscription_locator locator;
    };

    typedef chain::block::ptr_list block_list;
    typedef resubscriber<const code&, const chain::transaction&> address_subscriber;
    typedef resubscriber<const code&, const chain::transaction&> stealth_subscriber;

    bool handle_reorganization(const code& ec, uint64_t fork_point,
        const block_list& new_blocks, const block_list&);
    void publish_blocks(uint32_t fork_point, const block_list& blocks);

    upgrade_mutex mutex_;
};

} // namespace server
} // namespace libbitcoin

#endif
