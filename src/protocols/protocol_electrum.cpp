/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

#include <algorithm>
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace interface;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_electrum::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Blockchain methods.
    SUBSCRIBE_RPC(handle_blockchain_block_header, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_block_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_RPC(handle_blockchain_headers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_estimate_fee, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_relay_fee, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_balance, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_history, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_mempool, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_listunspent, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_subscribe, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_unsubscribe, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_transaction_broadcast, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_transaction_get, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_get_merkle, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_transaction_id_from_pos, _1, _2, _3, _4, _5);

    // Server methods
    SUBSCRIBE_RPC(handle_server_add_peer, _1, _2, _3);
    SUBSCRIBE_RPC(handle_server_banner, _1, _2);
    SUBSCRIBE_RPC(handle_server_donation_address, _1, _2);
    SUBSCRIBE_RPC(handle_server_features, _1, _2);
    SUBSCRIBE_RPC(handle_server_peers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_server_ping, _1, _2);
    ////SUBSCRIBE_RPC(handle_server_version, _1, _2, _3, _4);

    // Mempool methods.
    SUBSCRIBE_RPC(handle_mempool_get_fee_histogram, _1, _2);
    protocol_rpc<channel_electrum>::start();
}

// Handlers (blockchain).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_block_header(const code& ec,
    rpc_interface::blockchain_block_header, double ,
    double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_block_headers(const code& ec,
    rpc_interface::blockchain_block_headers, double ,
    double , double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_headers_subscribe(const code& ec,
    rpc_interface::blockchain_headers_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = query.to_header(query.get_top_confirmed_hash());
    const auto height = query.get_height(link);
    const auto header = query.get_header(link);
    if (height.is_terminal() || !header)
    {
        send_code(error::not_found);
        return;
    }

    // See protocol_native::to_hex().
    std::string hex(two * chain::header::serialized_size(), '\0');
    stream::out::fast sink{ hex };
    write::base16::fast writer{ sink };
    header->to_data(writer);
    BC_ASSERT(writer);

    // TODO: subscribe to block confirmation (idempotent).
    // TODO: upon notification send just the notifying header.
    // TODO: it is client responsiblity to deal with reorgs and race gaps.
    send_result(value_t
        {
            object_t
            {
                { "height", height.value },
                { "hex", hex }
            }
        }, 256, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_estimate_fee(const code& ec,
    rpc_interface::blockchain_estimate_fee, double number,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    // TODO: estimate fees from blocks based on expected block inclusion.
    // TODO: this can be computed from recent blocks and cached by the server.
    // TODO: update the cache before boradcasting header notifications.
    send_result(number, 70, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_relay_fee(const code& ec,
    rpc_interface::blockchain_relay_fee) NOEXCEPT
{
    if (stopped(ec))
        return;

    // TODO: implement from [node] config, removed in protocol 1.6.
    send_result(0.00000001, 70, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_scripthash_get_balance(const code& ec,
    rpc_interface::blockchain_scripthash_get_balance,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_history(const code& ec,
    rpc_interface::blockchain_scripthash_get_history,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_mempool(const code& ec,
    rpc_interface::blockchain_scripthash_get_mempool,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_listunspent(const code& ec,
    rpc_interface::blockchain_scripthash_listunspent,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_subscribe(const code& ec,
    rpc_interface::blockchain_scripthash_subscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_unsubscribe(const code& ec,
    rpc_interface::blockchain_scripthash_unsubscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_broadcast(const code& ec,
    rpc_interface::blockchain_transaction_broadcast,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_get(const code& ec,
    rpc_interface::blockchain_transaction_get, const std::string& ,
    bool ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_get_merkle(const code& ec,
    rpc_interface::blockchain_transaction_get_merkle, const std::string& ,
    double ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_id_from_pos(const code& ec,
    rpc_interface::blockchain_transaction_id_from_pos, double ,
    double , bool ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

// Handlers (server).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_server_add_peer(const code& ec,
    rpc_interface::server_add_peer, const interface::object_t& ) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_banner(const code& ec,
    rpc_interface::server_banner) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(network_settings().user_agent, 70, BIND(complete, _1));
}

void protocol_electrum::handle_server_donation_address(const code& ec,
    rpc_interface::server_donation_address) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_features(const code& ec,
    rpc_interface::server_features) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_peers_subscribe(const code& ec,
    rpc_interface::server_peers_subscribe) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_ping(const code& ec,
    rpc_interface::server_ping) NOEXCEPT
{
    if (stopped(ec)) return;
    send_code(error::not_implemented);
}

// Handlers (mempool).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_mempool_get_fee_histogram(const code& ec,
    rpc_interface::mempool_get_fee_histogram) NOEXCEPT
{
    if (stopped(ec))
        return;

    // TODO: requires tx pool metadata graph.
    send_result(value_t
        {
            array_t
            {
                array_t{ 1, 1024 },
                array_t{ 2, 2048 },
                array_t{ 4, 4096 }
            }
        }, 256, BIND(complete, _1));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
