/*
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
#include <bitcoin/server/service/fullnode.hpp>

#include <bitcoin/server/config.hpp>
#include <bitcoin/server/node_impl.hpp>
#include <bitcoin/server/service/fetch_x.hpp>

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;

void fullnode_fetch_history(node_impl& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    payment_address payaddr;
    uint32_t from_height;
    if (!unwrap_fetch_history_args(payaddr, from_height, request))
        return;
    // TODO: Slows down queries!
    //log_debug(LOG_WORKER) << "fetch_history("
    //    << payaddr.encoded() << ", from_height=" << from_height << ")";
    fetch_history(node.blockchain(), node.transaction_indexer(),
        payaddr,
        std::bind(send_history_result, _1, _2, request, queue_send),
        from_height);
}

} // namespace server
} // namespace libbitcoin

