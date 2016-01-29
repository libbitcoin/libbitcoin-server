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
#include <bitcoin/server/interface/address.hpp>

#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/message/incoming_message.hpp>
#include <bitcoin/server/message/outgoing_message.hpp>
#include <bitcoin/server/message/notifier.hpp>
#include <bitcoin/server/server_node.hpp>
#include "utility.hpp"

namespace libbitcoin {
namespace server {

using namespace bc::wallet;
using std::placeholders::_1;
using std::placeholders::_2;

void address::fetch_history2(server_node& node, const incoming_message& request,
    send_handler handler)
{
    uint32_t from_height;
    payment_address address;

    if (!unwrap_fetch_history_args(address, from_height, request))
        return;

    const auto hande_fetch_history =
        std::bind(send_history_result,
            _1, _2, request, handler);

    ////fetch_history(node.query(), node.transaction_indexer(), address,
    ////    hande_fetch_history, from_height);
}

void address::subscribe(notifier& notifier,
    const incoming_message& request, send_handler handler)
{
    notifier.subscribe(request, handler);
}

void address::renew(notifier& notifier,
    const incoming_message& request, send_handler handler)
{
    notifier.renew(request, handler);
}

} // namespace server
} // namespace libbitcoin
