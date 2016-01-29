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
#include <bitcoin/server/interface/transaction_pool.hpp>

#include <cstdint>
#include <cstddef>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include "utility.hpp"

namespace libbitcoin {
namespace server {
    
using namespace chain;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

void transaction_pool::validate(server_node& node,
    const incoming_message& request, send_handler handler)
{
    ////transaction tx;
    ////if (tx.from_data(request.data()))
    ////    node.transaction_pool().validate(tx,
    ////        std::bind(transaction_pool::transaction_validated,
    ////            _1, _2, _3, _4, request, handler));
    ////else
    ////    transaction_validated(error::bad_stream, transaction(), hash_digest(),
    ////        index_list(), request, handler);
}

void transaction_pool::validated(const code& ec, const transaction& tx,
    const hash_digest& tx_hash, const index_list& unconfirmed,
    const incoming_message& request, send_handler handler)
{
    data_chunk result(code_size + unconfirmed.size() * index_size);
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    for (const auto unconfirmed_index: unconfirmed)
    {
        BITCOIN_ASSERT(unconfirmed_index <= max_uint32);
        const auto index32 = static_cast<uint32_t>(unconfirmed_index);
        serial.write_4_bytes_little_endian(index32);
    }

    BITCOIN_ASSERT(serial.iterator() == result.end());
    log::debug(LOG_REQUEST)
        << "transaction_pool.validate() finished. Sending response: ec="
        << ec.message();

    outgoing_message response(request, result);
    handler(response);
}

void transaction_pool::fetch_transaction(server_node& node,
    const incoming_message& request, send_handler handler)
{
    hash_digest tx_hash;
    if (!unwrap_fetch_transaction_args(tx_hash, request))
        return;

    log::debug(LOG_REQUEST) << "transaction_pool.fetch_transaction("
        << encode_hash(tx_hash) << ")";

    ////node.transaction_pool().fetch(tx_hash,
    ////    std::bind(transaction_fetched,
    ////        _1, _2, request, handler));
}

} // namespace server
} // namespace libbitcoin
