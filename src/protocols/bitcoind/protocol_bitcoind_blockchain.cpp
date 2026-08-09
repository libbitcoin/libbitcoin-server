/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/server/protocols/protocol_bitcoind_blockchain.hpp>

#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

// Isolate the subgroup dispatch metaprogramming to this translation unit.
template class network::rpc::dispatcher<
    server::interface::bitcoind_blockchain>;

namespace server {

template class protocol_bitcoind_dispatch<interface::bitcoind_blockchain>;

#define CLASS protocol_bitcoind_blockchain
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

// bitcoind getblock verbosity levels (doc/JSON-RPC-interface.md).
// Unscoped for implicit conversion to the parsed level.
enum block_verbosity : size_t
{
    /// Serialized block, hex-encoded.
    hex = 0,

    /// Block object listing txids.
    hashed = 1,

    /// Block object embedding full tx objects.
    verbose = 2
};

// bitcoind defines only the "basic" (neutrino) block filter type.
constexpr auto basic_filter = "basic";

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_blockchain::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_best_block_hash, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_chain_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_filter, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_hash, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_header, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_chain_tx_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_tx_out, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_tx_out_set_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prune_block_chain, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_save_mempool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_tx_out_set, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_chain, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_dump_tx_out_set, _1, _2);
    SUBSCRIBE_BITCOIND(handle_load_tx_out_set, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_out_proof, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_tx_out_proof, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_from_peer, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_chain_states, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_chain_tips, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_deployment_info, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_descriptor_activity, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_difficulty, _1, _2);
    SUBSCRIBE_BITCOIND(handle_precious_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_blocks, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wait_for_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wait_for_block_height, _1, _2);
    SUBSCRIBE_BITCOIND(handle_wait_for_new_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_ancestors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_cluster, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_descendants, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_entry, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_mempool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_spending_prevout, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_mempool, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Blockchain methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_blockchain::handle_get_best_block_hash(const code& ec,
    rpc_interface::get_best_block_hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto hash = archive().get_top_confirmed_hash();
    send_result(encode_hash(hash), two * system::hash_size);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block(const code& ec,
    rpc_interface::get_block, const std::string& blockhash,
    double verbosity) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::invalid_argument);
        return true;
    }

    size_t level{};
    if (!to_integer(level, verbosity) || level > block_verbosity::verbose)
    {
        send_error(error::invalid_argument);
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(hash);
    const auto block = query.get_block(link, witness);
    if (!block)
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    if (level == block_verbosity::hex)
    {
        send_text(to_text(*block, block->serialized_size(witness), witness));
        return true;
    }

    auto model = level == block_verbosity::hashed ?
        value_from(bitcoind_hashed(*block)) :
        value_from(bitcoind_verbose(*block));

    inject_block_context(model.as_object(), query, link, block->header());
    send_result(std::move(model), two * block->serialized_size(witness));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_chain_info(const code& ec,
    rpc_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t out{};
    if (!chain_info(out, archive(), network_settings().pruned_node(),
        is_current_chain(true)))
    {
        send_error(database::error::integrity);
        return true;
    }

    send_result(std::move(out), 512);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_count(const code& ec,
    rpc_interface::get_block_count) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto top = archive().get_top_confirmed();
    send_result(top, 20);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_filter(const code& ec,
    rpc_interface::get_block_filter, const std::string& blockhash,
    const std::string& filtertype) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (filtertype != basic_filter)
    {
        send_error(error::invalid_argument);
        return true;
    }

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    const auto link = query.to_header(hash);
    data_chunk filter{};
    hash_digest filter_header{};
    if (!query.get_filter_body(filter, link) ||
        !query.get_filter_head(filter_header, link))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    send_result(object_t
    {
        { "filter", encode_base16(filter) },
        { "header", encode_hash(filter_header) }
    }, two * filter.size());
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_hash(const code& ec,
    rpc_interface::get_block_hash, double height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    size_t block_height{};
    if (!to_integer(block_height, height))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_confirmed(block_height);
    if (link.is_terminal())
    {
        send_error(error::not_found);
        return true;
    }

    send_result(encode_hash(query.get_header_key(link)), two * hash_size);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_header(const code& ec,
    rpc_interface::get_block_header, const std::string& blockhash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(hash);
    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    if (!verbose)
    {
        send_text(to_text(*header, chain::header::serialized_size()));
        return true;
    }

    auto out = header_to_bitcoind(*header);
    out["nTx"] = query.get_tx_count(link);
    inject_block_context(out, query, link, *header);
    send_result(value{ std::move(out) }, 512);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_stats(const code& ec,
    rpc_interface::get_block_stats, const value_t&, const array_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

// The window tx count is summed over the window (cost is linear in the window
// requested by the caller); txcount is cumulative from genesis (chain walk).
bool protocol_bitcoind_blockchain::handle_get_chain_tx_stats(const code& ec,
    rpc_interface::get_chain_tx_stats, double nblocks,
    const std::string& blockhash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    auto link = query.to_confirmed(query.get_top_confirmed());

    // The block defaults to the confirmed top, and must be on the chain.
    if (!blockhash.empty())
    {
        hash_digest hash{};
        if (!decode_hash(hash, blockhash))
        {
            send_error(error::invalid_argument);
            return true;
        }

        link = query.to_header(hash);
    }

    size_t height{};
    if (!query.get_height(height, link) || query.to_confirmed(height) != link)
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    // The default window is one month of blocks, bounded by the block height.
    constexpr auto month_seconds = 30_size * 24 * 60 * 60;
    const auto month = month_seconds / system_settings().block_spacing_seconds;
    size_t window{};
    if (nblocks < 0)
    {
        window = std::min(month, floored_subtract(height, one));
    }
    else
    {
        if (!to_integer(window, nblocks) ||
            (is_nonzero(window) && window >= height))
        {
            send_error(error::invalid_argument);
            return true;
        }
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    object_t result
    {
        { "time", header->timestamp() },
        { "txcount", query.get_branch_tx_count(link) },
        { "window_final_block_hash", encode_hash(query.get_header_key(link)) },
        { "window_final_block_height", height },
        { "window_block_count", window }
    };

    if (is_nonzero(window))
    {
        const auto first = floored_subtract(height, window);
        const auto past = query.to_confirmed(first);
        const auto interval = floored_subtract(median_time_past(query, link),
            median_time_past(query, past));

        size_t txs{};
        for (auto index = add1(first); index <= height; ++index)
            txs += query.get_tx_count(query.to_confirmed(index));

        result.emplace("window_interval", interval);
        result.emplace("window_tx_count", txs);

        if (is_nonzero(interval))
            result.emplace("txrate", to_floating(txs) / interval);
    }

    send_result(std::move(result), 256);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_tx_out(const code& ec,
    rpc_interface::get_tx_out, const std::string& txid, double n,
    bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    uint32_t index{};
    hash_digest hash{};
    if (!decode_hash(hash, txid) || !to_integer(index, n))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto output_link = query.to_output(hash, index);

    // bitcoind returns json null for a missing or confirmed-spent output; with
    // mempool ignored this matches gettxout's include_mempool=false semantics
    // (is_spent would also count unconfirmed/conflicting/invalid-block spenders).
    if (output_link.is_terminal() || query.is_confirmed_spent(output_link))
    {
        send_result({}, 42);
        return true;
    }

    const auto output = query.get_output(output_link);
    if (!output)
    {
        send_result({}, 42);
        return true;
    }

    // Output's tx must exist.
    const auto tx_link = query.to_tx(hash);
    if (tx_link.is_terminal())
    {
        send_error(error::server_error);
        return true;
    }

    // Derive header from top for consistent depth result (also cheaper).
    const auto top = query.get_top_confirmed();
    const auto header_link = query.to_confirmed(top);

    size_t height{};
    const auto strong = query.get_tx_height(height, tx_link);
    const auto depth = strong ? add1(floored_subtract(top, height)) : zero;
    const auto coins = to_floating(output->value()) /
        chain::satoshi_per_bitcoin;

    send_result(object_t
    {
        { "bestblock", encode_hash(query.get_header_key(header_link)) },
        { "confirmations", depth },
        { "value", coins },
        { "scriptPubKey", value_from(bitcoind(output->script())) },
        { "coinbase", query.is_coinbase(tx_link) }
    }, 256);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_tx_out_set_info(const code& ec,
    rpc_interface::get_tx_out_set_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_prune_block_chain(const code& ec,
    rpc_interface::prune_block_chain, double) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_save_mempool(const code& ec,
    rpc_interface::save_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_scan_tx_out_set(const code& ec,
    rpc_interface::scan_tx_out_set, const std::string&,
    const array_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_verify_chain(const code& ec,
    rpc_interface::verify_chain, double, double) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // no-op, store is reliable (integrity is guarded by the flush lock).
    send_result(value{ true }, 8);
    return true;
}

bool protocol_bitcoind_blockchain::handle_dump_tx_out_set(const code& ec,
    rpc_interface::dump_tx_out_set) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_load_tx_out_set(const code& ec,
    rpc_interface::load_tx_out_set) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

// The proof is a serialized merkle block (as the p2p merkleblock message and
// bitcoind's gettxoutproof). The block defaults to that confirming the first
// txid; libbitcoin archives all txs, so no txindex catch-up is needed.
bool protocol_bitcoind_blockchain::handle_get_tx_out_proof(const code& ec,
    rpc_interface::get_tx_out_proof, const array_t& txids,
    const std::string& blockhash) NOEXCEPT
{
    using namespace messages::peer;

    if (stopped(ec))
        return false;

    if (txids.empty())
    {
        send_error(error::invalid_argument);
        return true;
    }

    std::unordered_set<hash_digest> targets{};
    for (const auto& item: txids)
    {
        hash_digest hash{};
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())) ||
            !targets.insert(hash).second)
        {
            send_error(error::invalid_argument);
            return true;
        }
    }

    const auto& query = archive();
    auto link = query.find_confirmed_block(*targets.begin());

    // The block may be specified, otherwise the first txid determines it.
    if (!blockhash.empty())
    {
        hash_digest hash{};
        if (!decode_hash(hash, blockhash))
        {
            send_error(error::invalid_argument);
            return true;
        }

        link = query.to_header(hash);
    }

    if (!query.is_associated(link))
    {
        send_error(error::not_found);
        return true;
    }

    // Empty implies fault (the link resolves to an associated block).
    const auto keys = query.get_tx_keys(link);
    if (keys.empty())
    {
        send_error(database::error::integrity);
        return true;
    }

    // All targets must be in the block (matched in block order).
    // Not ranges algorithms, as the vector<bool> proxy iterator does not
    // satisfy indirectly_writable under libc++.
    std::vector<bool> match(keys.size());
    std::transform(keys.begin(), keys.end(), match.begin(),
        [&targets](const auto& key) NOEXCEPT
        {
            return targets.contains(key);
        });

    if (to_unsigned(std::count(match.begin(), match.end(), true)) !=
        targets.size())
    {
        send_error(error::not_found);
        return true;
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    const auto size = possible_narrow_cast<uint32_t>(keys.size());
    merkle_block merkle{ header, size, {}, {} };
    build_partial_merkle(merkle.flags, merkle.hashes, keys, match);

    const auto version = merkle_block::version_maximum;
    data_chunk out(merkle.size(version));
    merkle.serialize(version, out);
    send_text(encode_base16(out));
    return true;
}

// Verifies a serialized merkle block, returning the array of proven txids, or
// an empty array if the proof is invalid (as bitcoind).
bool protocol_bitcoind_blockchain::handle_verify_tx_out_proof(const code& ec,
    rpc_interface::verify_tx_out_proof, const std::string& proof) NOEXCEPT
{
    using namespace messages::peer;

    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, proof))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto version = merkle_block::version_maximum;
    const auto merkle = merkle_block::deserialize(version, data);
    if (!merkle)
    {
        send_error(error::invalid_argument);
        return true;
    }

    hash_digest root{};
    hashes matched{};
    std::vector<size_t> positions{};
    const auto ok = extract_partial_merkle(root, matched, positions,
        merkle->transactions, merkle->flags, merkle->hashes);

    // An invalid proof, or one not matching the block's merkle root, proves
    // nothing (empty result, as bitcoind).
    if (!ok || root != merkle->header->merkle_root())
    {
        send_result(array_t{}, 2);
        return true;
    }

    // The block must be on the confirmed chain with the proven tx count.
    const auto& query = archive();
    const auto link = query.to_header(merkle->header->hash());
    if (!query.is_confirmed_block(link) ||
        query.get_tx_count(link) != merkle->transactions)
    {
        send_error(error::not_found);
        return true;
    }

    array_t result(matched.size());
    std::ranges::transform(matched, result.begin(),
        [](const auto& hash) NOEXCEPT { return encode_hash(hash); });

    send_result(std::move(result), 32 * add1(matched.size()));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_from_peer(const code& ec,
    rpc_interface::get_block_from_peer) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_chain_states(const code& ec,
    rpc_interface::get_chain_states) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_chain_tips(const code& ec,
    rpc_interface::get_chain_tips) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Two indexes only: confirmed (active) and candidate (strongest headers);
    // weak/reorged branches live in the organizer tree and are not exposed.
    const auto& query = archive();
    const auto confirmed = query.get_top_confirmed();
    const auto confirmed_link = query.to_confirmed(confirmed);

    array_t tips
    {
        object_t
        {
            { "height", confirmed },
            { "hash", encode_hash(query.get_header_key(confirmed_link)) },
            { "branchlen", 0 },
            { "status", std::string{ "active" } }
        }
    };

    // The candidate is a distinct tip only where it forks above confirmed.
    const auto candidate = query.get_top_candidate();
    const auto branchlen = floored_subtract(candidate, query.get_fork());
    if (!is_zero(branchlen))
    {
        // Parent traversal is reorg-stable without the reorg lock. A hole in
        // the branch makes it headers-only, otherwise valid-headers; candidate
        // is the strongest valid chain so is never invalid or valid-fork.
        database::header_links branch{};
        const auto candidate_link = query.to_candidate(candidate);
        if (query.get_ancestry(branch, candidate_link, branchlen))
        {
            const auto present = std::all_of(branch.begin(), branch.end(),
                [&query](const auto& link) NOEXCEPT
                {
                    return query.is_associated(link);
                });

            tips.emplace_back(object_t
            {
                { "height", candidate },
                { "hash", encode_hash(query.get_header_key(candidate_link)) },
                { "branchlen", branchlen },
                { "status", std::string{ present ? "valid-headers" :
                    "headers-only" } }
            });
        }
    }

    send_result(std::move(tips), 256);
    return true;
}

// A frozen activation (bitcoind's "buried" type, which assumes depth).
// bitcoind reports it as active from one block below the activation height
// (the rules are enforced for the block that follows).
static void push_frozen(object_t& out, const std::string& name, bool enabled,
    size_t activation, size_t height) NOEXCEPT
{
    if (!enabled)
        return;

    out.emplace(name, object_t
    {
        { "type", std::string{ "buried" } },
        { "active", add1(height) >= activation },
        { "height", activation }
    });
}

// Deployments are configured, so this reads settings and needs no chain state.
// Taproot is excluded, as bitcoind froze it and no longer reports it here
// (btcd reports it under getblockchaininfo's bip9_softforks, which lnd reads).
bool protocol_bitcoind_blockchain::handle_get_deployment_info(const code& ec,
    rpc_interface::get_deployment_info, const std::string& blockhash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    auto link = query.to_confirmed(query.get_top_confirmed());

    // The block defaults to the confirmed top.
    if (!blockhash.empty())
    {
        hash_digest hash{};
        if (!decode_hash(hash, blockhash))
        {
            send_error(error::invalid_argument);
            return true;
        }

        link = query.to_header(hash);
    }

    size_t height{};
    if (!query.get_height(height, link))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    const auto& settings = system_settings();
    const auto& forks = settings.forks;
    object_t deployments{};
    push_frozen(deployments, "bip34", forks.bip34,
        settings.bip90_bip34_height, height);
    push_frozen(deployments, "bip66", forks.bip66,
        settings.bip90_bip66_height, height);
    push_frozen(deployments, "bip65", forks.bip65,
        settings.bip90_bip65_height, height);
    push_frozen(deployments, "csv", forks.bip68 && forks.bip112 &&
        forks.bip113, settings.bip9_bit0_active_checkpoint.height(), height);
    push_frozen(deployments, "segwit", forks.bip141 && forks.bip143 &&
        forks.bip147, settings.bip9_bit1_active_checkpoint.height(), height);

    send_result(object_t
    {
        { "hash", encode_hash(query.get_header_key(link)) },
        { "height", height },
        { "deployments", std::move(deployments) }
    }, 512);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_descriptor_activity(const code& ec,
    rpc_interface::get_descriptor_activity) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_difficulty(const code& ec,
    rpc_interface::get_difficulty) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    send_result(header->difficulty(), 20);
    return true;
}

bool protocol_bitcoind_blockchain::handle_precious_block(const code& ec,
    rpc_interface::precious_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_scan_blocks(const code& ec,
    rpc_interface::scan_blocks) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_wait_for_block(const code& ec,
    rpc_interface::wait_for_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_wait_for_block_height(const code& ec,
    rpc_interface::wait_for_block_height) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_wait_for_new_block(const code& ec,
    rpc_interface::wait_for_new_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_ancestors(const code& ec,
    rpc_interface::get_mempool_ancestors) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_cluster(const code& ec,
    rpc_interface::get_mempool_cluster) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_descendants(const code& ec,
    rpc_interface::get_mempool_descendants) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_entry(const code& ec,
    rpc_interface::get_mempool_entry) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_info(const code& ec,
    rpc_interface::get_mempool_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_raw_mempool(const code& ec,
    rpc_interface::get_raw_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_tx_spending_prevout(const code& ec,
    rpc_interface::get_tx_spending_prevout) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_blockchain::handle_import_mempool(const code& ec,
    rpc_interface::import_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
