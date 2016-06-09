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
#include <bitcoin/server/utility/notifier.hpp>

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::chain;

notifier::notifier()
{
    // TODO: connect subscriptions: work in progress.

    ////// Subscribe to blockchain reorganizations.
    ////node_.subscribe_blockchain(
    ////    std::bind(&block_service::handle_reorganization,
    ////        this, _1, _2, _3, _4));
}

void notifier::subscribe(const incoming& request, send_handler handler)
{
}

void notifier::renew(const incoming& request, send_handler handler)
{
}

bool notifier::handle_reorganization(const code& ec, uint64_t fork_point,
    const block_list& new_blocks, const block_list&)
{
    if (ec == bc::error::service_stopped)
        return false;

    if (ec)
    {
        log::warning(LOG_SERVER)
            << "Failure handling new block: " << ec.message();

        // Don't let a failure here prevent prevent future notifications.
        return true;
    }

    // TODO: publish_blocks

    return true;
}

void notifier::publish_blocks(uint32_t fork_point, const block_list& blocks)
{
}


} // namespace server
} // namespace libbitcoin
