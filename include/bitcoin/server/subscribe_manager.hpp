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
#ifndef LIBBITCOIN_SERVER_SUBSCRIBE_MANAGER_HPP
#define LIBBITCOIN_SERVER_SUBSCRIBE_MANAGER_HPP

#include <cstdint>
#include <unordered_map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/incoming_message.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/service/util.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class BCS_API subscribe_manager
{
public:
    subscribe_manager(server_node& node, const settings& settings);

    void subscribe(const incoming_message& request, send_handler handler);
    void renew(const incoming_message& request, send_handler handler);
    void scan(uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);

private:
    enum class subscribe_type
    {
        address = 0,
        stealth = 1
    };

    struct subscription
    {
        binary prefix;
        boost::posix_time::ptime expiry_time;
        data_chunk client_origin;
        send_handler handler;
        subscribe_type type;
    };

    typedef std::vector<subscription> list;
    
    // Private class typedef so use a template function.
    template <typename AddressPrefix>
    bool deserialize_address(AddressPrefix& address, subscribe_type& type,
        const data_chunk& data)
    {
        auto deserial = make_deserializer(data.begin(), data.end());
        try
        {
            type = deserial.read_byte() == 0 ? subscribe_type::address :
                subscribe_type::stealth;
            auto bit_length = deserial.read_byte();
            auto blocks = deserial.read_data(binary::blocks_size(bit_length));
            address = AddressPrefix(bit_length, blocks);
        }
        catch (const end_of_stream&)
        {
            return false;
        }

        return deserial.iterator() == data.end();
    }

    void do_subscribe(const incoming_message& request, send_handler handler);
    void do_renew(const incoming_message& request, send_handler handler);
    void do_scan(uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);

    void post_updates(const wallet::payment_address& address,
        uint32_t height, const hash_digest& block_hash,
        const chain::transaction& tx);
    void post_stealth_updates(uint32_t prefix, uint32_t height,
        const hash_digest& block_hash, const chain::transaction& tx);

    code add(const incoming_message& request, send_handler handler);
    void sweep();

    threadpool pool_;
    dispatcher dispatch_;
    list subscriptions_;
    const settings& settings_;
};

} // namespace server
} // namespace libbitcoin

#endif
