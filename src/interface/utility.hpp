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
#ifndef LIBBITCOIN_SERVER_UTILITY_HPP
#define LIBBITCOIN_SERVER_UTILITY_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

// TODO: move to util or private class static in another location.

namespace libbitcoin {
namespace server {

static constexpr uint8_t spend_type = 1;
static constexpr uint8_t output_type = 0;
static constexpr size_t code_size = sizeof(uint32_t);
static constexpr size_t index_size = sizeof(uint32_t);
static constexpr size_t point_size = hash_size + sizeof(uint32_t);

// fetch_history stuff

bool BCS_API unwrap_fetch_history_args(wallet::payment_address& address,
    uint32_t& from_height, const incoming_message& request);

void BCS_API send_history_result(const code& ec,
    const bc::blockchain::block_chain::history& history,
    const incoming_message& request, send_handler handler);

// fetch_transaction stuff

bool BCS_API unwrap_fetch_transaction_args(hash_digest& hash,
    const incoming_message& request);

void BCS_API transaction_fetched(const code& ec, const chain::transaction& tx,
    const incoming_message& request, send_handler handler);

} // namespace server
} // namespace libbitcoin

#endif

