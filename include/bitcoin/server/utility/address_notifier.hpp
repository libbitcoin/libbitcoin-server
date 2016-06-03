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
#ifndef LIBBITCOIN_SERVER_ADDRESS_NOTIFIER_HPP
#define LIBBITCOIN_SERVER_ADDRESS_NOTIFIER_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

class BCS_API address_notifier
  : public enable_shared_from_base<address_notifier>
{
public:
    typedef std::shared_ptr<address_notifier> ptr;

    /// Construct an address notifier.
    address_notifier(server_node& node);

    /// This class is not copyable.
    address_notifier(const address_notifier&) = delete;
    void operator=(const address_notifier&) = delete;

    /// Subscribe to block and transaction notifications and send messages.
    bool start();

    /// Subscribe addresses to the notifier.
    void subscribe(const incoming& request, send_handler handler);

    /// Renew an existing subscription to the notifier.
    void renew(const incoming& request, send_handler handler);

private:
    struct subscription
    {
        binary prefix;
        send_handler handler;
        chain::subscribe_type type;
        boost::posix_time::ptime expiry_time;

        // client addressing
        data_chunk address1;
        data_chunk address2;
        bool delimited;
    };

    typedef std::vector<subscription> list;

    void scan(uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);
    void post_updates(const wallet::payment_address& address,
        uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);
    void post_stealth_updates(uint32_t prefix, uint32_t height,
        const hash_digest& block_hash, const chain::transaction& tx);

    void sweep();
    code add(const incoming& request, send_handler handler);
    boost::posix_time::ptime now();
    bool deserialize_address(binary& address, chain::subscribe_type& type,
        const data_chunk& data);

    server_node& node_;
    list subscriptions_;
    const settings& settings_;
};

} // namespace server
} // namespace libbitcoin

#endif
