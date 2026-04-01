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

#include <algorithm>
#include <map>
#include <ranges>
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

// github.com/spesmilo/electrum-protocol/blob/master/docs/protocol-methods.rst

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
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
    SUBSCRIBE_RPC(handle_blockchain_transaction_broadcast_package, _1, _2, _3, _4);
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
            if (subscribed_.load(std::memory_order_relaxed))
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

// Handlers (blockchain).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_blockchain_block_header(const code& ec,
    rpc_interface::blockchain_block_header, double height,
    double cp_height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t starting{};
    size_t waypoint{};
    if (!to_integer(starting, height) ||
        !to_integer(waypoint, cp_height))
    {
        send_code(error::invalid_argument);
        return;
    }

    blockchain_block_headers(starting, one, waypoint, false);
}

void protocol_electrum::handle_blockchain_block_headers(const code& ec,
    rpc_interface::blockchain_block_headers, double start_height, double count,
    double cp_height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::wrong_version);
        return;
    }

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

    if (!is_zero(cp_height) && !at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    blockchain_block_headers(starting, quantity, waypoint, true);
}

// Common implementation for blockchain_block_header/s.
void protocol_electrum::blockchain_block_headers(size_t starting,
    size_t quantity, size_t waypoint, bool multiplicity) NOEXCEPT
{
    const auto prove = !is_zero(quantity) && !is_zero(waypoint);
    const auto target = starting + sub1(quantity);
    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    using namespace system;

    // The documented requirement: `start_height + (count - 1) <= cp_height` is
    // ambiguous at count = 0 so guard must be applied to both args and prover.
    if (is_add_overflow(starting, quantity))
    {
        send_code(error::argument_overflow);
        return;
    }
    else if (starting > top)
    {
        send_code(error::not_found);
        return;
    }
    else if (prove && waypoint > top)
    {
        send_code(error::not_found);
        return;
    }
    else if (prove && target > waypoint)
    {
        send_code(error::target_overflow);
        return;
    }

    // Recommended to be at least one difficulty retarget period, e.g. 2016.
    // The maximum number of headers the server will return in single request.
    const auto maximum_headers = server_settings().electrum.maximum_headers;

    // Returned headers are assured to be contiguous despite intervening reorg.
    // No headers may be returned, which implies start > confirmed top block.
    const auto count = limit(quantity, maximum_headers);
    const auto links = query.get_confirmed_headers(starting, count);
    auto size = two * chain::header::serialized_size() * links.size();

    value_t value{ object_t{} };
    auto& result = std::get<object_t>(value.value());
    if (multiplicity)
    {
        result["max"] = maximum_headers;
        result["count"] = links.size();
    }
    else if (links.empty())
    {
        send_code(error::server_error);
        return;
    }

    if (at_least(electrum::version::v1_6))
    {
        array_t headers{};
        headers.reserve(links.size());
        for (const auto& link: links)
        {
            const auto header = query.get_wire_header(link);
            if (header.empty())
            {
                send_code(error::server_error);
                return;
            }

            headers.push_back(encode_base16(header));
        };

        if (multiplicity)
            result["headers"] = std::move(headers);
        else
            result["header"] = std::move(headers.front());
    }
    else
    {
        std::string headers(size, '\0');
        stream::out::fast sink{ headers };
        write::base16::fast writer{ sink };
        for (const auto& link: links)
        {
            if (!query.get_wire_header(writer, link))
            {
                send_code(error::server_error);
                return;
            }
        };

        result["hex"] = std::move(headers);
    }

    // There is a very slim chance of inconsistency given an intervening reorg
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
            [](const auto& hash) NOEXCEPT { return encode_hash(hash); });

        result["branch"] = std::move(branch);
        result["root"] = encode_hash(root);
        size += two * hash_size * add1(proof.size());
    }

    send_result(std::move(value), size + 42u, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_headers_subscribe(const code& ec,
    rpc_interface::blockchain_headers_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);

    // This is unlikely but possible due to a race condition during reorg.
    if (link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    const auto header = query.get_wire_header(link);
    if (header.empty())
    {
        send_code(error::server_error);
        return;
    }

    subscribed_.store(true, std::memory_order_relaxed);
    send_result(
    {
        object_t
        {
            { "height", top },
            { "hex", encode_base16(header) }
        }
    }, 256, BIND(complete, _1));
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
    }, 100, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_estimate_fee(const code& ec,
    rpc_interface::blockchain_estimate_fee, double ,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    ////const auto mode_enabled = at_least(electrum::version::v1_6);

    ////send_result(number, 70, BIND(complete, _1));
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_relay_fee(const code& ec,
    rpc_interface::blockchain_relay_fee) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0) ||
         at_least(electrum::version::v1_6))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(node_settings().minimum_fee_rate, 42, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_scripthash_get_balance(const code& ec,
    rpc_interface::blockchain_scripthash_get_balance,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_history(const code& ec,
    rpc_interface::blockchain_scripthash_get_history,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_get_mempool(const code& ec,
    rpc_interface::blockchain_scripthash_get_mempool,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    ////const auto sort = at_least(electrum::version::v1_6);

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_list_unspent(const code& ec,
    rpc_interface::blockchain_scripthash_list_unspent,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_subscribe(const code& ec,
    rpc_interface::blockchain_scripthash_subscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_scripthash_unsubscribe(const code& ec,
    rpc_interface::blockchain_scripthash_unsubscribe,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4_2))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_broadcast(const code& ec,
    rpc_interface::blockchain_transaction_broadcast,
    const std::string& raw_tx) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: implement error_object.
    // Changed in version 1.1: return error vs. bitcoind result.
    // Previously it returned text string (bitcoind message) in the error case.
    ////const auto error_object = at_least(electrum::version::v1_1);

    data_chunk tx_data{};
    if (!decode_base16(tx_data, raw_tx))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto tx = to_shared<chain::transaction>(tx_data, true);
    if (!tx->is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    // TODO: handle just as any peer annoucement, validate and relay.
    // TODO: requires tx pool in order to validate against unconfirmed txs.
    constexpr auto confirmable = false;
    if (!confirmable)
    {
        send_code(error::unconfirmable_transaction);
        return;
    }

    constexpr auto size = two * hash_size;
    send_result(encode_base16(tx->hash(false)), size, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_broadcast_package(
    const code& ec, rpc_interface::blockchain_transaction_broadcast_package,
    const std::string& raw_txs, bool ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_6))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk txs_data{};
    if (!decode_base16(txs_data, raw_txs))
    {
        send_code(error::invalid_argument);
        return;
    }

    // TODO: consider whether to support the lousy package p2p protocol.
    constexpr auto confirmable = false;
    if (!confirmable)
    {
        send_code(error::unconfirmable_transaction);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_transaction_get(const code& ec,
    rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return;

    // TODO: changed in version 1.1: ignored height argument removed.
    // Requires additional same-name method implementation for v1.0.
    // This implies and override to channel_rpc<electrum>::dispatch().
    if ((!at_least(electrum::version::v1_0)) ||
        (!at_least(electrum::version::v1_2) && verbose))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto link = query.to_tx(hash);
    if (link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    if (!verbose)
    {
        const auto tx = query.get_wire_tx(link, true);
        if (tx.empty())
        {
            send_code(error::server_error);
            return;
        }

        send_result(encode_base16(tx), two * tx.size(), BIND(complete, _1));
        return;
    }

    const auto tx = query.get_transaction(link, true);
    if (!tx)
    {
        send_code(error::server_error);
        return;
    }

    auto value = value_from(bitcoind(*tx));
    if (!value.is_object())
    {
        send_code(error::server_error);
        return;
    }

    if (const auto header = query.to_strong(link); !header.is_terminal())
    {
        using namespace system;
        const auto top = query.get_top_confirmed();
        const auto height = query.get_height(header);
        const auto block_hash = query.get_header_key(header);

        uint32_t timestamp{};
        if (height.is_terminal() || (block_hash == null_hash) ||
            !query.get_timestamp(timestamp, header))
        {
            send_code(error::server_error);
            return;
        }

        // Floor manages race between getting confirmed top and height.
        const auto confirms = add1(floored_subtract(top, height.value));

        auto& transaction = value.as_object();
        transaction["in_active_chain"] = true;
        transaction["blockhash"] = encode_hash(block_hash);
        transaction["confirmations"] = confirms;
        transaction["blocktime"] = timestamp;
        transaction["time"] = timestamp;
    }

    // Verbose means whatever bitcoind returns for getrawtransaction, lolz.
    const auto size = tx->serialized_size(true);
    send_result(std::move(value), two * size, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_get_merkle(const code& ec,
    rpc_interface::blockchain_transaction_get_merkle, const std::string& tx_hash,
    double height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    size_t block_height{};
    if (!to_integer(block_height, height) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto block_link = query.to_confirmed(block_height);
    if (block_link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    auto hashes = query.get_tx_keys(block_link);
    if (hashes.empty())
    {
        send_code(error::server_error);
        return;
    }

    const auto index = find_position(hashes, hash);
    if (is_negative(index))
    {
        send_code(error::not_found);
        return;
    }

    using namespace chain;
    const auto position = to_unsigned(index);
    const auto proof = block::merkle_branch(index, std::move(hashes));

    array_t branch(proof.size());
    std::ranges::transform(proof, branch.begin(),
        [](const auto& hash) NOEXCEPT{ return encode_hash(hash); });

    send_result(
    {
        object_t
        {
            { "merkle", std::move(branch) },
            { "block_height", block_height },
            { "pos", position }
        }
    }, two * hash_size * add1(branch.size()), BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_id_from_pos(const code& ec,
    rpc_interface::blockchain_transaction_id_from_pos, double height,
    double tx_pos, bool merkle) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t position{};
    size_t block_height{};
    if (!to_integer(block_height, height) ||
        !to_integer(position, tx_pos))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto block_link = query.to_confirmed(block_height);
    const auto tx_link = query.get_position_tx(block_link, position);
    if (tx_link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    using namespace system;
    const auto hash = query.get_tx_key(tx_link);
    if (hash == null_hash)
    {
        send_code(error::server_error);
        return;
    }

    if (!merkle)
    {
        send_result(encode_hash(hash), two * hash_size, BIND(complete, _1));
        return;
    }

    auto hashes = query.get_tx_keys(block_link);
    if (hashes.empty())
    {
        send_code(error::server_error);
        return;
    }

    if (position >= hashes.size())
    {
        send_code(error::not_found);
        return;
    }

    using namespace chain;
    const auto proof = block::merkle_branch(position, std::move(hashes));

    array_t branch(proof.size());
    std::ranges::transform(proof, branch.begin(),
        [](const auto& hash) NOEXCEPT { return encode_hash(hash); });

    send_result(
    {
        object_t
        {
            { "tx_hash", encode_hash(hash) },
            { "merkle", std::move(branch) }
        }
    }, two * hash_size * add1(branch.size()), BIND(complete, _1));
}

// Handlers (server).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_server_add_peer(const code& ec,
    rpc_interface::server_add_peer, const interface::object_t& ) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_banner(const code& ec,
    rpc_interface::server_banner) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(options().banner_message, 42, BIND(complete, _1));
}

void protocol_electrum::handle_server_donation_address(const code& ec,
    rpc_interface::server_donation_address) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(options().donation_address, 42, BIND(complete, _1));
}

void protocol_electrum::handle_server_features(const code& ec,
    rpc_interface::server_features) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    const auto& query = archive();
    const auto genesis = query.to_confirmed(zero);
    if (genesis.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    const auto hash = query.get_header_key(genesis);
    if (hash == null_hash)
    {
        send_code(error::server_error);
        return;
    }

    send_result(object_t
    {
        { "genesis_hash", encode_hash(hash) },
        { "hosts", advertised_hosts() },
        { "hash_function", "sha256" },
        { "server_version", options().server_name },
        { "protocol_min", string_t{ version_to_string(minimum) } },
        { "protocol_max", string_t{ version_to_string(maximum) } },
        { "pruning", null_t{} }
    }, 1024, BIND(complete, _1));
}

// This is not actually a subscription method.
void protocol_electrum::handle_server_peers_subscribe(const code& ec,
    rpc_interface::server_peers_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    send_code(error::not_implemented);
}

void protocol_electrum::handle_server_ping(const code& ec,
    rpc_interface::server_ping) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::wrong_version);
        return;
    }

    // Any receive, including ping, resets the base channel inactivity timer.
    send_result(null_t{}, 42, BIND(complete, _1));
}

// Handlers (mempool).
// ----------------------------------------------------------------------------

void protocol_electrum::handle_mempool_get_fee_histogram(const code& ec,
    rpc_interface::mempool_get_fee_histogram) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: requires tx pool metadata graph.
    send_code(error::not_implemented);
}

void protocol_electrum::handle_mempool_get_info(const code& ec,
    rpc_interface::mempool_get_info) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: requires tx pool metadata graph.
    send_code(error::not_implemented);
}

// utilities
// ----------------------------------------------------------------------------

// One of each type allowed for given host, last writer wins if more than one.
object_t protocol_electrum::advertised_hosts() const NOEXCEPT
{
    std::map<string_t, object_t> map{};

    for (const auto& bind: options().advertise_binds)
        if (!bind.host().empty())
            map[bind.host()]["tcp_port"] = bind.port();

    for (const auto& safe: options().advertise_safes)
        if (!safe.host().empty())
            map[safe.host()]["ssl_port"] = safe.port();

    object_t hosts{};
    for (const auto& [host, object]: map)
        hosts[host] = object;

    if (hosts.empty()) return
    {
        { "tcp_port", null_t{} },
        { "ssl_port", null_t{} }
    };

    return hosts;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
