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
#include <ranges>
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

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3));

    // Blockchain methods.
    SUBSCRIBE_RPC(handle_blockchain_block_header, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_block_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_RPC(handle_blockchain_headers_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_estimate_fee, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_blockchain_relay_fee, _1, _2);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_balance, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_history, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_get_mempool, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_scripthash_list_unspent, _1, _2, _3);
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

void protocol_electrum::stopping(const code& ec) NOEXCEPT
{
    // Unsubscriber race is ok.
    BC_ASSERT(stranded());
    unsubscribe_events();
    protocol_rpc<channel_electrum>::stopping(ec);
}

// Handlers (event subscription).
// ----------------------------------------------------------------------------

bool protocol_electrum::handle_event(const code&, node::chase event_,
    node::event_value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case node::chase::suspend:
        {
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Utility.
// ----------------------------------------------------------------------------

// TODO: move to system/math.
template <typename Integer, if_integer<Integer> = true>
bool to_integer(Integer& out, double value) NOEXCEPT
{
    if (!std::isfinite(value))
        return false;

    double integral{};
    const double fractional = std::modf(value, &integral);
    if (fractional != 0.0)
        return false;

    if (integral > static_cast<double>(system::maximum<Integer>) ||
        integral < static_cast<double>(system::minimum<Integer>))
        return false;

    BC_PUSH_WARNING(NO_STATIC_CAST)
    out = static_cast<Integer>(integral);
    BC_POP_WARNING()
    return true;
}

// TODO: centralize in server (also used in bitcoind and native interfaces).
template <typename Object, typename ...Args>
std::string to_hex(const Object& object, size_t size, Args&&... args) NOEXCEPT
{
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    BC_ASSERT(writer);
    return out;
}

// Handlers (blockchain).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_block_header(const code& ec,
    rpc_interface::blockchain_block_header, double height,
    double cp_height) NOEXCEPT
{
    handle_blockchain_block_headers(ec, {}, height, 1, cp_height);
}

void protocol_electrum::handle_blockchain_block_headers(const code& ec,
    rpc_interface::blockchain_block_headers, double start_height, double count,
    double cp_height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    size_t quantity{};
    size_t waypoint{};
    size_t starting{};
    if (!to_integer(quantity, count) ||
        !to_integer(waypoint, cp_height) ||
        !to_integer(starting, start_height))
    {
        send_code(error::invalid_argument);
        return;
    }

    if (is_add_overflow(starting, quantity))
    {
        send_code(error::argument_overflow);
        return;
    }

    // The documented requirement: `start_height + (count - 1) <= cp_height` is
    // ambiguous at count = 0 so guard must be applied to both args and prover.
    const auto target = starting + sub1(quantity);
    const auto prove = !is_zero(quantity) && !is_zero(waypoint);
    if (prove && target > waypoint)
    {
        send_code(error::target_overflow);
        return;
    }

    // Recommended to be at least one difficulty retarget period, e.g. 2016.
    // The maximum number of headers the server will return in single request.
    const auto maximum = server_settings().electrum.maximum_headers;

    // Returned headers are assured to be contiguous despite intervening reorg.
    // No headers may be returned, which implies start > confirmed top block.
    const auto& query = archive();
    const auto bound = limit(quantity, maximum);
    const auto links = query.get_confirmed_headers(starting, bound);
    constexpr auto header_size = chain::header::serialized_size();
    auto size = two * header_size * links.size();

    // Fetch and serialize headers.
    array_t headers{};
    headers.reserve(links.size());
    for (const auto& link: links)
    {
        if (const auto header = query.get_header(link); header)
        {
            // TODO: optimize by query directly returning wire serialization.
            headers.push_back(to_hex(*header, header_size));
        }
        else
        {
            send_code(error::server_error);
            return;
        }
    };

    value_t value
    {
        object_t
        {
            { "count", uint64_t{ quantity } },
            { "headers", std::move(headers) },
            { "max", maximum }
        }
    };

    // There is a very slim change of inconsistency given an intervening reorg
    // because of get_merkle_root_and_proof() use of height-based calculations.
    // This is acceptable as it must be verified by caller in any case.
    if (prove)
    {
        hashes proof{};
        hash_digest root{};
        if (const auto code = query.get_merkle_root_and_proof(root, proof,
            target, waypoint))
        {
            send_code(code);
            return;
        }

        array_t branch(proof.size());
        std::ranges::transform(proof, branch.begin(),
            [](const auto& hash) { return encode_base16(hash); });

        auto& result = std::get<object_t>(value.value());
        result["root"] = encode_base16(root);
        result["branch"] = std::move(branch);
        size += two * hash_size * add1(proof.size());
    }

    send_result(std::move(value), size + 42u, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_headers_subscribe(const code& ec,
    rpc_interface::blockchain_headers_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);

    // This is unlikely but possible due to a race condition during reorg.
    if (!link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_code(error::server_error);
        return;
    }

    // TODO: signal header subscription.

    // TODO: idempotent subscribe to chase::organized via session/chaser/node.
    // TODO: upon notification send just the header notified by the link.
    // TODO: it is client responsibility to deal with reorgs and race gaps.
    send_result(value_t
        {
            object_t
            {
                { "height", uint64_t{ top } },
                { "hex", to_hex(*header, chain::header::serialized_size()) }
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
    // TODO: update the cache before broadcasting header notifications.
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

void protocol_electrum::handle_blockchain_scripthash_list_unspent(const code& ec,
    rpc_interface::blockchain_scripthash_list_unspent,
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

// This is not actually a subscription method.
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
