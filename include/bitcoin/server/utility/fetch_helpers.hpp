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
#ifndef LIBBITCOIN_SERVER_FETCH_HELPERS_HPP
#define LIBBITCOIN_SERVER_FETCH_HELPERS_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

static BC_CONSTEXPR size_t code_size = sizeof(uint32_t);
static BC_CONSTEXPR size_t index_size = sizeof(uint32_t);
static BC_CONSTEXPR size_t point_size = hash_size + sizeof(uint32_t);

// fetch_history stuff

bool BCS_API unwrap_fetch_history_args(wallet::payment_address& address,
    size_t& from_height, const message& request);

void BCS_API send_history_result(const code& ec,
    const chain::history_compact::list& history, const message& request,
    send_handler handler);

// fetch_transaction stuff

bool BCS_API unwrap_fetch_transaction_args(hash_digest& hash,
    const message& request);

void BCS_API transaction_fetched(const code& ec, transaction_ptr tx,
    size_t height, size_t position, const message& request,
    send_handler handler);

} // namespace server
} // namespace libbitcoin

#endif

