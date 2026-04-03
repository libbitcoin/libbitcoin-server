/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <atomic>
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

// Start.
// ----------------------------------------------------------------------------
// electrum-protocol.readthedocs.io/en/latest/protocol-methods.html

void protocol_electrum::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));

    // Header methods.
    SUBSCRIBE_RPC(handle_blockchain_number_of_blocks_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_block_get_chunk, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_block_get_header, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_block_header, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_block_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_RPC(handle_blockchain_headers_subscribe, _1, _2);

    // Fee methods.
    SUBSCRIBE_RPC(handle_blockchain_estimate_fee, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_relay_fee, _1, _2);

    // Address methods.
    SUBSCRIBE_RPC(handle_blockchain_utxo_get_address, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_address_get_balance, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_address_get_history, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_address_get_mempool, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_address_list_unspent, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_address_subscribe, _1, _2, _3);

    // Scripthash methods.
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_balance, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_history, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_mempool, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_list_unspent, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_subscribe, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_unsubscribe, _1, _2, _3);

    // Transaction methods.
    SUBSCRIBE_RPC(handle_blockchain_transaction_broadcast, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_transaction_broadcast_package, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_get, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_get_merkle, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_id_from_position, _1, _2, _3, _4, _5);

    // Server methods.
    SUBSCRIBE_RPC(handle_server_add_peer, _1, _2, _3);
    SUBSCRIBE_RPC(handle_server_banner, _1, _2);
    SUBSCRIBE_RPC(handle_server_donation_address, _1, _2);
    SUBSCRIBE_RPC(handle_server_features, _1, _2);
    SUBSCRIBE_RPC(handle_server_peers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_server_ping, _1, _2);
    ////SUBSCRIBE_RPC(handle_server_version, _1, _2, _3, _4);

    // Mempool methods.
    SUBSCRIBE_RPC(handle_mempool_get_fee_histogram, _1, _2);
    SUBSCRIBE_RPC(handle_mempool_get_info, _1, _2);
    protocol_rpc<channel_electrum>::start();
}

void protocol_electrum::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Unsubscription is asynchronous, race is ok.
    unsubscribe_events();
    protocol_rpc<channel_electrum>::stopping(ec);
}

// Handlers (event subscription).
// ----------------------------------------------------------------------------

bool protocol_electrum::handle_event(const code&, node::chase event_,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case node::chase::organized:
        {
            if (subscribed_height_.load(std::memory_order_relaxed))
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST(do_height, std::get<node::header_t>(value));
            }

            if (subscribed_header_.load(std::memory_order_relaxed))
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST(do_header, std::get<node::header_t>(value));
            }

            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Notifier for handle_blockchain_number_of_blocks_subscribe events.
void protocol_electrum::do_height(node::header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto& query = archive();
    const auto height = query.get_height(link);

    if (height.is_terminal())
    {
        LOGF("Electrum::do_height, object not found (" << link << ").");
        return;
    }

    send_notification("blockchain.numblocks.subscribe",
    {
        array_t{ height.value }
    }, 48, BIND(complete, _1));
}

// Notifier for blockchain_headers_subscribe events.
void protocol_electrum::do_header(node::header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto& query = archive();
    const auto height = query.get_height(link);
    const auto header = query.get_wire_header(link);

    if (height.is_terminal())
    {
        LOGF("Electrum::do_header, object not found (" << link << ").");
        return;
    }

    send_notification("blockchain.headers.subscribe",
    {
        object_t
        {
            { "height", height.value },
            { "hex", encode_base16(header) }
        }
    }, 64, BIND(complete, _1));
}

} // namespace server
} // namespace libbitcoin
