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
#ifndef LIBBITCOIN_SERVER_FETCH_X_HPP
#define LIBBITCOIN_SERVER_FETCH_X_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

// fetch_history stuff

bool BCS_API unwrap_fetch_history_args(
    bc::wallet::payment_address& payaddr, uint32_t& from_height,
    const incoming_message& request);

void BCS_API send_history_result(
    const std::error_code& ec, const bc::chain::history_list& history,
    const incoming_message& request, queue_send_callback queue_send);

// fetch_transaction stuff

bool BCS_API unwrap_fetch_transaction_args(
    bc::hash_digest& tx_hash, const incoming_message& request);

void BCS_API transaction_fetched(const std::error_code& ec,
    const bc::chain::transaction& tx,
    const incoming_message& request, queue_send_callback queue_send);

} // namespace server
} // namespace libbitcoin

#endif

