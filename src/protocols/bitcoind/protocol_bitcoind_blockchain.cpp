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

#include <ranges>
#include <unordered_set>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

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
    verbose = 2,

    /// Adds per-input prevout context and per-tx fee.
    prevouts = 3
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
    SUBSCRIBE_BITCOIND(handle_get_tx_out_set_info, _1, _2, _3, _4, _5);
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
    SUBSCRIBE_BITCOIND(handle_get_descriptor_activity, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_difficulty, _1, _2);
    SUBSCRIBE_BITCOIND(handle_precious_block, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_scan_blocks, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_BITCOIND(handle_wait_for_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_wait_for_block_height, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_wait_for_new_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mempool_ancestors, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mempool_cluster, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_descendants, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mempool_entry, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_mempool_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_mempool, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_tx_spending_prevout, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_import_mempool, _1, _2);
    subscribe_chase(BIND(handle_chase, _1, _2));
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

void protocol_bitcoind_blockchain::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    unsubscribe_chase();
    waiter_.reset();
    protocol_bitcoind_dispatch<rpc_interface>::stopping(ec);
}

// Blockchain methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_blockchain::handle_get_best_block_hash(const code& ec,
    rpc_interface::get_best_block_hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto hash = archive().get_top_confirmed_hash();
    send_result(encode_hash(hash));
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    int64_t requested{};
    if (!to_integer(requested, verbosity))
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    // bitcoind clamps out of range verbosity.
    const auto level = limit<size_t>(requested, block_verbosity::prevouts);

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(hash);
    if (level == block_verbosity::hex)
    {
        size_t size{};
        if (!query.get_block_size(size, link, witness))
        {
            send_error(error::bitcoind::invalid_address_or_key, blockhash);
            return true;
        }

        const auto turbo = database_settings().turbo;
        if (!send_hex(two * size, [&](const data_slab& out) NOEXCEPT
        {
            return query.get_wire_block<write::base16::fast>(out, link,
                witness, turbo);
        }))
            send_error(error::bitcoind::misc_error);

        return true;
    }

    const auto block = query.get_block(link, witness);
    if (!block)
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash);
        return true;
    }

    auto model = level == block_verbosity::hashed ?
        value_from(bitcoind_hashed(*block)) :
        value_from(bitcoind_verbose(*block, flags_));

    const auto& settings = system_settings();
    const auto& header = block->header();
    inject_block_context(model.as_object(), query, settings, link, header);

    if (level >= block_verbosity::verbose)
    {
        const auto populated = query.populate_with_metadata(*block);
        auto entry = model.as_object().at("tx").as_array().begin();
        std::ranges::for_each(*block->transactions_ptr(),
            [&](const auto& tx) NOEXCEPT
        {
            auto& object = entry->as_object();
            inject_tx_scripts(object, *tx, p2kh_, p2sh_, witness_);
            if (populated && !tx->is_coinbase())
            {
                if (level == block_verbosity::prevouts)
                    inject_tx_prevouts(object, query, *tx, p2kh_, p2sh_,
                        witness_, flags_);

                object["fee"] =
                    tx->fee() / to_floating(chain::satoshi_per_bitcoin);
            }

            ++entry;
        });
    }

    send_result(std::move(model));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_chain_info(const code& ec,
    rpc_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t out{};
    if (!chain_info(out, archive(), system_settings(),
        node_settings().limited_blocks, is_current_chain(true)))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    send_result(std::move(out));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_count(const code& ec,
    rpc_interface::get_block_count) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto top = archive().get_top_confirmed();
    send_result(top);
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
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    const auto link = query.to_header(hash);
    data_chunk filter{};
    hash_digest filter_header{};
    if (!query.get_filter_body(filter, link) ||
        !query.get_filter_head(filter_header, link))
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash);
        return true;
    }

    send_result(object_t
    {
        { "filter", encode_base16(filter) },
        { "header", encode_hash(filter_header) }
    });
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_confirmed(block_height);
    if (link.is_terminal())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    send_result(encode_hash(query.get_header_key(link)));
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(hash);
    if (!verbose)
    {
        constexpr auto size = two * chain::header::serialized_size();
        if (!send_hex(size, [&](const data_slab& out) NOEXCEPT
        {
            stream::out::fast sink{ out };
            write::base16::fast writer{ sink };
            return query.get_wire_header(writer, link) && writer;
        }))
            send_error(error::bitcoind::invalid_address_or_key, blockhash);

        return true;
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash);
        return true;
    }

    auto model = value_from(bitcoind(*header));
    auto& out = model.as_object();
    out["nTx"] = query.get_tx_count(link);
    inject_block_context(out, query, system_settings(), link, *header);
    send_result(std::move(model));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_stats(const code& ec,
    rpc_interface::get_block_stats, const value_t& hash_or_height,
    const array_t& stats) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    database::header_link link{};
    if (std::holds_alternative<string_t>(hash_or_height.value()))
    {
        hash_digest hash{};
        if (!decode_hash(hash, std::get<string_t>(hash_or_height.value())))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_header(hash);
    }
    else if (std::holds_alternative<number_t>(hash_or_height.value()))
    {
        size_t height{};
        if (!to_integer(height, std::get<number_t>(hash_or_height.value())))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_confirmed(height);
    }
    else
    {
        send_error(error::bitcoind::type_error);
        return true;
    }

    if (!query.is_confirmed_block(link))
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    size_t height{};
    if (!query.get_height(height, link))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    // Fees require prevout values, populated from the store.
    const auto block = query.get_block(link, true);
    if (!block || !query.populate_without_metadata(*block))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    const auto& settings = system_settings();
    const auto subsidy = chain::block::subsidy(height,
        settings.subsidy_interval_blocks, settings.initial_subsidy(),
        settings.forks.bip42);

    const auto median = median_time(query, settings, link);
    auto result = block_stats(*block, height, median, subsidy);

    // An empty selection returns all statistics, otherwise the named subset.
    if (stats.empty())
    {
        send_result(std::move(result));
        return true;
    }

    object_t selected{};
    for (const auto& stat: stats)
    {
        if (!std::holds_alternative<string_t>(stat.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        const auto& name = std::get<string_t>(stat.value());
        const auto it = result.find(name);
        if (it == result.cend())
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        selected.emplace(name, it->second);
    }

    send_result(std::move(selected));
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
    auto link = query.to_top_confirmed();

    // The block defaults to the confirmed top, and must be on the chain.
    if (!blockhash.empty())
    {
        hash_digest hash{};
        if (!decode_hash(hash, blockhash))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_header(hash);
    }

    if (!query.is_confirmed_block(link))
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash);
        return true;
    }

    size_t height{};
    if (!query.get_height(height, link))
    {
        send_error(error::bitcoind::internal_error);
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
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(error::bitcoind::internal_error);
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
        // The pinned ancestry makes the window walk reorg-stable.
        database::header_links branch{};
        if (!query.get_ancestry(branch, link, add1(window)))
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }

        const auto& settings = system_settings();
        const auto start = median_time(query, settings, branch.back());
        const auto end = median_time(query, settings, link);
        const auto interval = floored_subtract(end, start);

        size_t txs{};
        for (size_t index{}; index < window; ++index)
            txs += query.get_tx_count(branch.at(index));

        result.emplace("window_interval", interval);
        result.emplace("window_tx_count", txs);

        if (is_nonzero(interval))
            result.emplace("txrate", to_floating(txs) / interval);
    }

    send_result(std::move(result));
    return true;
}

// The mempool is the set of pooled and confirmable txs.
bool protocol_bitcoind_blockchain::handle_get_tx_out(const code& ec,
    rpc_interface::get_tx_out, const std::string& txid, double n,
    bool include_mempool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    uint32_t index{};
    hash_digest hash{};
    if (!decode_hash(hash, txid) || !to_integer(index, n))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    // Return only unspent output.
    const auto& query = archive();
    const auto output_link = query.to_output(hash, index);
    if (output_link.is_terminal() || query.is_confirmed_spent(output_link) ||
        (include_mempool && query.is_confirmable_spent(output_link)))
    {
        send_result(null_t{});
        return true;
    }

    size_t top{};
    const auto header_link = query.to_top_confirmed();
    const auto output = query.get_output(output_link);
    const auto tx_link = query.to_output_tx(output_link);
    if (!output || tx_link.is_terminal() || !query.get_height(top, header_link))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    // Return only confirmed output, or mempool output if included.
    size_t depth{};
    if (size_t height{}; query.get_tx_height(height, tx_link))
    {
        depth = add1(floored_subtract(top, height));
    }
    else if (!include_mempool || !query.is_pooled(tx_link) ||
        !query.is_confirmable_tx(tx_link))
    {
        send_result(null_t{});
        return true;
    }

    const auto coins = to_floating(output->value()) /
        chain::satoshi_per_bitcoin;

    send_result(object_t
    {
        { "bestblock", encode_hash(query.get_header_key(header_link)) },
        { "confirmations", depth },
        { "value", coins },
        { "scriptPubKey", script_public_key(output->script()) },
        { "coinbase", query.is_coinbase(tx_link) }
    });
    return true;
}

// The response defers to completion of the store scan (see dispatch). This is
// an administrative query, expected to run long, performed off the strand.
bool protocol_bitcoind_blockchain::handle_get_tx_out_set_info(const code& ec,
    rpc_interface::get_tx_out_set_info, const std::string& hash_type,
    const value_t& hash_or_height, bool use_index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    set_hash type{};
    if (hash_type == "hash_serialized_3")
        type = set_hash::serialized;
    else if (hash_type == "muhash")
        type = set_hash::muhash;
    else if (hash_type == "none")
        type = set_hash::none;
    else
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& query = archive();
    auto height = query.get_top_confirmed();
    if (!std::holds_alternative<null_t>(hash_or_height.value()))
    {
        // The scan commits at any height, so only the contradiction applies.
        if (!use_index)
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        database::header_link link{};
        if (std::holds_alternative<string_t>(hash_or_height.value()))
        {
            hash_digest hash{};
            if (!decode_hash(hash, std::get<string_t>(hash_or_height.value())))
            {
                send_error(error::bitcoind::invalid_parameter);
                return true;
            }

            link = query.to_header(hash);
        }
        else if (std::holds_alternative<number_t>(hash_or_height.value()))
        {
            if (!to_integer(height,
                std::get<number_t>(hash_or_height.value())))
            {
                send_error(error::bitcoind::misc_error);
                return true;
            }

            link = query.to_confirmed(height);
        }
        else
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        if (!query.is_confirmed_block(link))
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }

        if (!query.get_height(height, link))
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }
    }

    PARALLEL(do_get_tx_out_set_info, type, height, gate());
    return true;
}

void protocol_bitcoind_blockchain::do_get_tx_out_set_info(set_hash type,
    size_t height, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    const auto link = query.to_confirmed(height);

    // The pinned ancestry excludes genesis, absent from the set.
    database::header_links branch{};
    if (!query.get_ancestry(branch, link, height))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    code ec{};
    hash_digest digest{};
    database::unspent_totals totals{};
    const auto turbo = database_settings().turbo;

    if (type == set_hash::none)
        ec = query.get_unspent_totals(stopping_, totals, branch, turbo);
    else if (type == set_hash::muhash)
        ec = query.get_unspent_muhash(stopping_, totals, digest, branch, turbo);
    else
        ec = query.get_unspent_serialized(stopping_, totals, digest, branch,
            turbo);

    if (ec)
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    // A reorganization across the pinned top voids the scan.
    if (!query.is_confirmed_block(link))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    object_t result
    {
        { "height", height },
        { "bestblock", encode_hash(query.get_header_key(link)) },
        { "transactions", totals.transactions },
        { "txouts", totals.outputs },

        // bitcoind's per-utxo accounting fiction (50 byte overhead).
        { "bogosize", 50u * totals.outputs + totals.script_bytes },

        // The coin-serialized set size (bitcoind estimates leveldb size).
        { "disk_size", totals.coin_bytes },
        { "total_amount", to_floating(totals.value) /
            chain::satoshi_per_bitcoin }
    };

    // Coins issued but not in the set are permanently unspendable.
    const auto& settings = system_settings();
    const auto issued = total_subsidy(settings, height);
    const auto bitcoin = to_floating(chain::satoshi_per_bitcoin);
    result.emplace("total_unspendable_amount",
        floored_subtract(issued, totals.value) / bitcoin);

    object_t amounts{};
    if (block_info(amounts, query, settings, link, height))
        result.emplace("block_info", std::move(amounts));

    if (type == set_hash::serialized)
        result.emplace("hash_serialized_3", encode_hash(digest));

    if (type == set_hash::muhash)
        result.emplace("muhash", encode_hash(digest));

    POST(complete_object, std::move(result), gate);
}

bool protocol_bitcoind_blockchain::handle_prune_block_chain(const code& ec,
    rpc_interface::prune_block_chain, double) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::misc_error);
    return true;
}

bool protocol_bitcoind_blockchain::handle_save_mempool(const code& ec,
    rpc_interface::save_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

// The response defers to completion of the utxo scan (see dispatch). The
// expansion may require a million derivations and without the address index
// the scan walks the full utxo set (recovery-grade), both off the strand.
bool protocol_bitcoind_blockchain::handle_scan_tx_out_set(const code& ec,
    rpc_interface::scan_tx_out_set, const std::string& action,
    const array_t& scanobjects) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Each scan completes with its response, so there is never one to report.
    if (action == "status")
    {
        send_result(null_t{});
        return true;
    }

    if (action == "abort")
    {
        send_result(value{ false });
        return true;
    }

    if (action != "start")
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    PARALLEL(do_scan_tx_out_set, emplace_shared<array_t>(scanobjects), gate());
    return true;
}

void protocol_bitcoind_blockchain::do_scan_tx_out_set(
    const std::shared_ptr<array_t>& objects, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    chain::scripts scripts{};
    for (const auto& item: *objects)
    {
        if (!expand_scan_object(scripts, item, context_))
        {
            POST(fail, error::bitcoind::invalid_address_or_key, gate);
            return;
        }
    }

    // The needle set is keyed by output script hash (the address index key).
    // The utxo set excludes unspendable outputs, so those needles are
    // dropped, and the set deduplicates repeated expressions.
    std::unordered_set<hash_digest> keys{};
    for (const auto& script: scripts)
        if (!script.is_unspendable())
            keys.insert(sha256_hash(script.to_data(false)));

    code ec{};
    size_t txouts{};
    database::unspent_coins coins{};
    size_t top{};
    const auto& query = archive();
    const auto link = query.to_top_confirmed();
    const auto bip30 = query.envelope().forks.bip30;

    if (!query.get_height(top, link))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    if (query.address_enabled())
    {
        // Indexed candidates avoid the full scan, so the scanned coin count
        // reflects only examined candidates (bitcoind scans the whole set).
        for (const auto& script: scripts)
        {
            const auto data = script.to_data(false);
            const auto key = sha256_hash(data);
            if (!keys.erase(key))
                continue;

            database::unspent_outputs outs{};
            ec = query.get_confirmed_unspent(stopping_, outs, key,
                database_settings().turbo);
            if (ec)
                break;

            for (const auto& utxo: outs)
            {
                // The genesis output is excluded from the set.
                if (is_zero(utxo.height) && is_zero(utxo.position))
                    continue;

                ++txouts;
                coins.push_back({ false, utxo.out, utxo.height,
                    is_zero(utxo.position), data });
            }
        }
    }
    else
    {
        // The pinned ancestry excludes genesis, absent from the set.
        database::header_links branch{};
        if (!query.get_ancestry(branch, link, top))
        {
            ec = database::error::integrity;
        }
        else
        {
            // The store matches by script hash over the scan (the set size is
            // the count of coins examined, as bitcoind scans the whole set).
            ec = query.get_unspent_matches(stopping_, coins, txouts, keys,
                branch, database_settings().turbo);
        }
    }

    if (ec)
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    // A reorganization across the pinned top voids the scan.
    if (!query.is_confirmed_block(link))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    POST(complete_object, scan_result(coins, query, scripts, top, txouts, bip30,
        p2kh_, p2sh_, witness_), gate);
}

bool protocol_bitcoind_blockchain::handle_verify_chain(const code& ec,
    rpc_interface::verify_chain, double, double) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // no-op, store is reliable (integrity is guarded by the flush lock).
    send_result(value{ true });
    return true;
}

bool protocol_bitcoind_blockchain::handle_dump_tx_out_set(const code& ec,
    rpc_interface::dump_tx_out_set) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_blockchain::handle_load_tx_out_set(const code& ec,
    rpc_interface::load_tx_out_set) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_tx_out_proof(const code& ec,
    rpc_interface::get_tx_out_proof, const array_t& txids,
    const std::string& blockhash) NOEXCEPT
{
    using namespace messages::peer;

    if (stopped(ec))
        return false;

    if (txids.empty())
    {
        send_error(error::bitcoind::invalid_parameter);
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
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }
    }

    const auto& query = archive();
    auto link = query.find_confirmed_block(*targets.cbegin());

    // The block may be specified, otherwise the first txid determines it.
    if (!blockhash.empty())
    {
        hash_digest hash{};
        if (!decode_hash(hash, blockhash))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_header(hash);
    }

    if (!query.is_associated(link))
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    // Empty implies fault (the link resolves to an associated block).
    const auto keys = query.get_tx_keys(link);
    if (keys.empty())
    {
        send_error(error::bitcoind::internal_error);
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

    if (to_unsigned(std::count(match.cbegin(), match.cend(), true)) !=
        targets.size())
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    const auto size = possible_narrow_cast<uint32_t>(keys.size());
    merkle_block merkle{ header, size, {}, {} };
    build_partial_merkle(merkle.flags, merkle.hashes, keys, match);

    const auto version = merkle_block::version_maximum;
    if (!send_hex(two * merkle.size(version), [&](const data_slab& out) NOEXCEPT
    {
        data_chunk data(merkle.size(version));
        if (!merkle.serialize(version, data))
            return false;

        stream::out::fast sink{ out };
        write::base16::fast writer{ sink };
        writer.write_bytes(data);
        return !!writer;
    }))
        send_error(error::bitcoind::internal_error);

    return true;
}

// Verifies a serialized merkle block, returning the array of proven txids, or
// an empty array if the proof is invalid.
bool protocol_bitcoind_blockchain::handle_verify_tx_out_proof(const code& ec,
    rpc_interface::verify_tx_out_proof, const std::string& proof) NOEXCEPT
{
    using namespace messages::peer;

    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, proof))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto version = merkle_block::version_maximum;
    const auto merkle = merkle_block::deserialize(version, data);
    if (!merkle)
    {
        send_error(error::bitcoind::misc_error);
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
        send_result(array_t{});
        return true;
    }

    // The block must be on the confirmed chain with the proven tx count.
    const auto& query = archive();
    const auto link = query.to_header(merkle->header->hash());
    if (!query.is_confirmed_block(link) ||
        query.get_tx_count(link) != merkle->transactions)
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    array_t result(matched.size());
    std::ranges::transform(matched, result.begin(),
        [](const auto& hash) NOEXCEPT { return encode_hash(hash); });

    send_result(std::move(result));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_from_peer(const code& ec,
    rpc_interface::get_block_from_peer) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_chain_states(const code& ec,
    rpc_interface::get_chain_states) NOEXCEPT
{
    if (stopped(ec))
        return false;

    size_t candidate{};
    const auto& query = archive();
    const auto candidate_link = query.to_top_candidate();
    if (!query.get_height(candidate, candidate_link))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    auto entry = chain_states_entry(query, query.to_top_confirmed(), 1.0, true);
    if (entry.empty())
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    array_t states{ std::move(entry) };

    if (!query.is_coalesced())
    {
        // Validated above the fork point does not imply confirmability.
        size_t fork{};
        const auto span = query.get_validated_fork(fork);
        const auto validated = progress(fork + span.size(), candidate);
        entry = chain_states_entry(query, candidate_link, validated, false);
        if (entry.empty())
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }

        states.push_back(std::move(entry));
    }

    send_result(object_t
    {
        // bitcoind OB1 error ("headers" wants height).
        { "headers", candidate },
        { "chainstates", states }
    });
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_chain_tips(const code& ec,
    rpc_interface::get_chain_tips) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Two indexes only: confirmed (active) and candidate (strongest headers);
    // weak/reorged branches live in the organizer tree and are not exposed.
    size_t confirmed{};
    const auto& query = archive();
    const auto confirmed_link = query.to_top_confirmed();
    if (!query.get_height(confirmed, confirmed_link))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

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

    // The candidate is a distinct top only where it forks above confirmed.
    size_t candidate{};
    const auto candidate_link = query.to_top_candidate();
    if (!query.get_height(candidate, candidate_link))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    const auto branchlen = floored_subtract(candidate, query.get_fork());
    if (!is_zero(branchlen))
    {
        // Parent traversal is reorg-stable without the reorg lock. A hole in
        // the branch makes it headers-only, otherwise valid-headers; candidate
        // is the strongest valid chain so is never invalid or valid-fork.
        database::header_links branch{};
        if (query.get_ancestry(branch, candidate_link, branchlen))
        {
            const auto present = std::all_of(branch.cbegin(), branch.cend(),
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

    send_result(std::move(tips));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_deployment_info(const code& ec,
    rpc_interface::get_deployment_info, const std::string& blockhash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    auto link = query.to_top_confirmed();

    // The block defaults to the confirmed top.
    if (!blockhash.empty())
    {
        hash_digest hash{};
        if (!decode_hash(hash, blockhash))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_header(hash);
        if (link.is_terminal())
        {
            send_error(error::bitcoind::invalid_address_or_key, blockhash);
            return true;
        }
    }

    size_t height{};
    if (!query.get_height(height, link))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    send_result(deployment_info(query, system_settings(), link, height));
    return true;
}

// The mempool option is meaningless here (no mempool), always applied false.
bool protocol_bitcoind_blockchain::handle_get_descriptor_activity(
    const code& ec, rpc_interface::get_descriptor_activity,
    const array_t& blockhashes, const array_t& scanobjects,
    bool include_mempool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (include_mempool && !archive().address_enabled())
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    PARALLEL(do_get_descriptor_activity, emplace_shared<array_t>(blockhashes),
        emplace_shared<array_t>(scanobjects), include_mempool, gate());
    return true;
}

void protocol_bitcoind_blockchain::do_get_descriptor_activity(
    const std::shared_ptr<array_t>& blockhashes,
    const std::shared_ptr<array_t>& scanobjects, bool include_mempool,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    chain::scripts derived{};
    for (const auto& item: *scanobjects)
    {
        if (!expand_scan_object(derived, item, context_))
        {
            POST(fail, error::bitcoind::invalid_address_or_key, gate);
            return;
        }
    }

    std::unordered_set<std::string> watch{};
    for (const auto& script: derived)
        watch.insert(encode_base16(script.to_data(false)));

    const auto& query = archive();
    array_t activity{};
    for (const auto& item: *blockhashes)
    {
        hash_digest hash{};
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            POST(fail, error::bitcoind::invalid_parameter, gate);
            return;
        }

        constexpr auto witness = true;
        const auto link = query.to_header(hash);
        const auto block = query.get_block(link, witness);
        if (!block)
        {
            POST(fail, error::bitcoind::invalid_address_or_key, gate);
            return;
        }

        size_t height{};
        if (!query.get_height(height, link))
        {
            POST(fail, error::bitcoind::internal_error, gate);
            return;
        }

        // Confirmed spends are unconditional.
        if (!query.populate_without_metadata(*block))
        {
            POST(fail, error::bitcoind::internal_error, gate);
            return;
        }

        inject_activity(activity, *block, height, encode_hash(hash), watch,
            p2kh_, p2sh_, witness_, flags_);
    }

    if (include_mempool)
    {
        database::tx_links links{};
        std::unordered_set<database::tx_link::integer> seen{};
        for (const auto& script: derived)
        {
            database::histories history{};
            const auto key = sha256_hash(script.to_data(false));
            if (query.get_unconfirmed_history(stopping_, history, key,
                max_size_t, database_settings().turbo))
            {
                POST(fail, error::bitcoind::internal_error, gate);
                return;
            }

            for (const auto& item: history)
            {
                const auto link = query.to_tx(item.tx.hash());
                if (seen.insert(link.value).second)
                    links.push_back(link.value);
            }
        }

        for (const auto& link: links)
        {
            const auto tx = query.get_transaction(link, true);
            if (!tx || !query.populate_without_metadata(*tx))
            {
                POST(fail, error::bitcoind::internal_error, gate);
                return;
            }

            inject_activity(activity, *tx, {}, watch, p2kh_, p2sh_, witness_,
                flags_);
        }
    }

    POST(complete_object, object_t{ { "activity", std::move(activity) } }, gate);
}

bool protocol_bitcoind_blockchain::handle_get_difficulty(const code& ec,
    rpc_interface::get_difficulty) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto header = query.get_header(query.to_top_confirmed());
    if (!header)
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    send_result(header->difficulty());
    return true;
}

// The response defers to reorganize completion (see dispatch).
bool protocol_bitcoind_blockchain::handle_precious_block(const code& ec,
    rpc_interface::precious_block, const std::string& blockhash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    // The block may be cached (tied branch), so the organizer resolves it.
    prioritize(hash, BIND(handle_prioritize, _1, _2));
    return true;
}

void protocol_bitcoind_blockchain::handle_prioritize(const code& ec,
    size_t) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_precious_block, ec);
}

// bitcoind returns null, having preferred the block where it could.
void protocol_bitcoind_blockchain::do_precious_block(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec == database::error::not_found)
        send_error(error::bitcoind::invalid_address_or_key);
    else if (ec)
        send_error(error::bitcoind::internal_error);
    else
        send_result(null_t{});
}

// Exact index matching produces no false positives to optionally filter.
bool protocol_bitcoind_blockchain::handle_scan_blocks(const code& ec,
    rpc_interface::scan_blocks, const std::string& action,
    const array_t& scanobjects, double start_height, double stop_height,
    const std::string& filtertype, const object_t&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (action != "start")
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    if (filtertype != basic_filter)
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    if (scanobjects.empty())
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    const auto top = query.get_top_confirmed();
    size_t from{};
    auto to = top;
    if (!to_integer(from, start_height) ||
        (stop_height >= 0 && !to_integer(to, stop_height)))
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    to = std::min(to, top);
    if (from > to)
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    chain::scripts scripts{};
    for (const auto& item: scanobjects)
    {
        if (!expand_scan_object(scripts, item, context_))
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }
    }

    // The pinned ancestry makes the range walk reorg-stable.
    database::header_links branch{};
    if (!query.get_ancestry(branch, query.to_confirmed(to), add1(to - from)))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    array_t relevant{};
    for (auto it = branch.crbegin(); it != branch.crend(); ++it)
    {
        const auto hash = query.get_header_key(*it);
        neutrino::block_filter filter{ hash, {} };
        if (!query.get_filter_body(filter.filter, *it))
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }

        if (neutrino::match_filter(filter, scripts))
            relevant.emplace_back(encode_hash(hash));
    }

    send_result(object_t
    {
        { "from_height", from },
        { "to_height", to },
        { "relevant_blocks", std::move(relevant) },
        { "completed", true }
    });
    return true;
}

// The response defers to the chase event or the timeout (long poll). The
// saved request context carries the deferred send (see dispatch).
bool protocol_bitcoind_blockchain::handle_wait_for_block(const code& ec,
    rpc_interface::wait_for_block, const std::string& blockhash,
    double timeout) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    arm_wait({ wait::block, {}, hash }, timeout);
    return true;
}

bool protocol_bitcoind_blockchain::handle_wait_for_block_height(const code& ec,
    rpc_interface::wait_for_block_height, const double height,
    double timeout) NOEXCEPT
{
    if (stopped(ec))
        return false;

    size_t target{};
    if (!to_integer(target, height))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    arm_wait({ wait::height, target, {} }, timeout);
    return true;
}

bool protocol_bitcoind_blockchain::handle_wait_for_new_block(const code& ec,
    rpc_interface::wait_for_new_block, double timeout,
    const std::string& current_tip) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // A stated top that is no longer current completes the wait immediately.
    hash_digest given{};
    if (!current_tip.empty() && decode_hash(given, current_tip) &&
        (given != archive().get_top_confirmed_hash()))
    {
        send_top();
        return true;
    }

    const auto next = add1(archive().get_top_confirmed());
    arm_wait({ wait::new_block, next, {} }, timeout);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_ancestors(const code& ec,
    rpc_interface::get_mempool_ancestors, const std::string& txid,
    bool verbose) NOEXCEPT
{
    if (stopped(ec)) return false;
    return get_mempool_relatives(txid, verbose, true);
}

bool protocol_bitcoind_blockchain::handle_get_mempool_cluster(const code& ec,
    rpc_interface::get_mempool_cluster) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_descendants(const code& ec,
    rpc_interface::get_mempool_descendants, const std::string& txid,
    bool verbose) NOEXCEPT
{
    if (stopped(ec)) return false;
    return get_mempool_relatives(txid, verbose, false);
}

bool protocol_bitcoind_blockchain::get_mempool_relatives(
    const std::string& txid, bool verbose, bool ancestors) NOEXCEPT
{
    hash_digest hash{};
    if (!decode_hash(hash, txid))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    PARALLEL(do_get_mempool_relatives, hash, verbose, ancestors, gate());
    return true;
}

void protocol_bitcoind_blockchain::do_get_mempool_relatives(
    const hash_digest& hash, bool verbose, bool ancestors,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto link = to_mempool_tx(hash);
    if (link.is_terminal())
    {
        POST(fail, error::bitcoind::invalid_address_or_key, gate);
        return;
    }

    const auto& query = archive();
    const auto links = ancestors ? query.to_confirmable_ancestors(link) :
        query.to_confirmable_descendants(link);

    value_t result{};
    if (!mempool_txs(result, links, verbose))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    POST(complete_value, std::move(result), gate);
}

bool protocol_bitcoind_blockchain::handle_get_mempool_entry(const code& ec,
    rpc_interface::get_mempool_entry, const std::string& txid) NOEXCEPT
{
    if (stopped(ec)) return false;

    hash_digest hash{};
    if (!decode_hash(hash, txid))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    PARALLEL(do_get_mempool_entry, hash, gate());
    return true;
}

void protocol_bitcoind_blockchain::do_get_mempool_entry(
    const hash_digest& hash, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto link = to_mempool_tx(hash);
    if (link.is_terminal())
    {
        POST(fail, error::bitcoind::invalid_address_or_key, gate);
        return;
    }

    object_t entry{};
    if (!mempool_entry(entry, link))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    POST(complete_value, value_t{ std::move(entry) }, gate);
}

// The minimum is that sent to peers (feefilter), while the relay minimum is
// the configured rate.
bool protocol_bitcoind_blockchain::handle_get_mempool_info(const code& ec,
    rpc_interface::get_mempool_info) NOEXCEPT
{
    if (stopped(ec)) return false;

    const auto& settings = node_settings();
    const auto minimum = to_floating(minimum_fee_rate()) /
        chain::satoshi_per_bitcoin;
    const auto relay = to_floating(settings.minimum_fee_rate_()) /
        chain::satoshi_per_bitcoin;

    PARALLEL(do_get_mempool_info, minimum, relay, settings.minimum_bump_rate,
        gate());
    return true;
}

// The pool is stored, so usage is the size of its tables, and the maximum is
// the space available to the store (shared with all tables and the volume).
void protocol_bitcoind_blockchain::do_get_mempool_info(double minimum,
    double relay, double increment, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    size_t space{};
    database::fee_rates rates{};
    const auto& query = archive();
    if (!query.get_confirmable_fees(rates) ||
        !database::file::space(space, database_settings().path))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    size_t bytes{};
    uint64_t fee{};
    for (const auto& rate: rates)
    {
        bytes = ceilinged_add(bytes, rate.bytes);
        fee = ceilinged_add(fee, rate.fee);
    }

    const auto indexes = ceilinged_add(query.spends_size(), query.wtxid_size());
    const auto usage = ceilinged_add(query.pool_size(), indexes);
    const auto total_fee = to_floating(fee) / chain::satoshi_per_bitcoin;

    POST(complete_value, value_t
    {
        object_t
        {
            { "loaded", true },
            { "size", rates.size() },
            { "bytes", bytes },
            { "usage", usage },
            { "total_fee", total_fee },
            { "maxmempool", space },
            { "mempoolminfee", minimum },
            { "minrelaytxfee", relay },
            { "incrementalrelayfee", increment },
            { "unbroadcastcount", 0 },
            { "fullrbf", false }
        }
    }, gate);
}

bool protocol_bitcoind_blockchain::handle_get_raw_mempool(const code& ec,
    rpc_interface::get_raw_mempool, bool verbose, bool mempool_sequence) NOEXCEPT
{
    if (stopped(ec)) return false;

    if (verbose && mempool_sequence)
    {
        send_error(error::bitcoind::invalid_params);
        return true;
    }

    PARALLEL(do_get_raw_mempool, verbose, mempool_sequence, gate());
    return true;
}

// There is no mempool sequence (zmq), so it is reported as constant.
void protocol_bitcoind_blockchain::do_get_raw_mempool(bool verbose,
    bool sequence, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    value_t txs{};
    database::tx_links links{};
    if (archive().get_confirmable_txs(links) ||
        !mempool_txs(txs, links, verbose))
    {
        POST(fail, error::bitcoind::internal_error, gate);
        return;
    }

    if (verbose || !sequence)
    {
        POST(complete_value, std::move(txs), gate);
        return;
    }

    POST(complete_value, value_t
    {
        object_t
        {
            { "txids", std::move(txs) },
            { "mempool_sequence", 1 }
        }
    }, gate);
}

// A tx is in the mempool if a stored instance of it is pooled and confirmable.
database::tx_link protocol_bitcoind_blockchain::to_mempool_tx(
    const hash_digest& hash) const NOEXCEPT
{
    const auto& query = archive();
    for (const auto& link: query.to_duplicates(hash))
        if (query.is_pooled(link) && query.is_confirmable_tx(link))
            return link;

    return {};
}

// An array of txids, or an object of entries keyed by txid (verbose).
bool protocol_bitcoind_blockchain::mempool_txs(value_t& out,
    const database::tx_links& links, bool verbose) const NOEXCEPT
{
    const auto& query = archive();
    if (verbose)
    {
        object_t entries{};
        for (const auto& link: links)
        {
            object_t entry{};
            if (!mempool_entry(entry, link))
                return false;

            const auto txid = encode_hash(query.get_tx_key(link));
            entries.emplace(txid, std::move(entry));
        }

        out = std::move(entries);
        return true;
    }

    array_t txids{};
    txids.reserve(links.size());
    for (const auto& link: links)
        txids.push_back(value_t{ encode_hash(query.get_tx_key(link)) });

    out = std::move(txids);
    return true;
}

// The entry height is that of the chain top, below the pooled context height.
bool protocol_bitcoind_blockchain::mempool_entry(object_t& out,
    const database::tx_link& link) const NOEXCEPT
{
    const auto& query = archive();

    uint32_t time{};
    database::fee_rate rate{};
    database::fee_totals ancestors{}, descendants{};
    size_t light{}, heavy{};
    if (!query.get_tx_fees(rate, link) ||
        !query.get_tx_sizes(light, heavy, link) ||
        !query.get_pooled_time(time, link) ||
        !query.get_ancestor_totals(ancestors, link) ||
        !query.get_descendant_totals(descendants, link))
        return false;

    const auto to_txids = [&query](const database::tx_links& links) NOEXCEPT
    {
        array_t txids{};
        txids.reserve(links.size());
        for (const auto& fk: links)
            txids.push_back(value_t{ encode_hash(query.get_tx_key(fk)) });

        return txids;
    };

    const auto to_btc = [](uint64_t satoshis) NOEXCEPT
    {
        return to_floating(satoshis) / chain::satoshi_per_bitcoin;
    };

    const object_t fees
    {
        { "base", to_btc(rate.fee) },
        { "modified", to_btc(rate.fee) },
        { "ancestor", to_btc(ancestors.fee) },
        { "descendant", to_btc(descendants.fee) }
    };

    out = object_t
    {
        { "vsize", rate.bytes },
        { "weight", chain::weighted_size(light, heavy) },
        { "time", time },
        { "height", sub1(rate.height) },
        { "descendantcount", descendants.count },
        { "descendantsize", descendants.bytes },
        { "ancestorcount", ancestors.count },
        { "ancestorsize", ancestors.bytes },
        { "wtxid", encode_hash(query.get_wtxid(link)) },
        { "fees", fees },
        { "depends", to_txids(query.to_confirmable_parents(link)) },
        { "spentby", to_txids(query.to_confirmable_children(link)) },
        { "bip125-replaceable", false },
        { "unbroadcast", false }
    };

    return true;
}

// The spender index is complete, so mempool_only defaults to false.
bool protocol_bitcoind_blockchain::handle_get_tx_spending_prevout(const code& ec,
    rpc_interface::get_tx_spending_prevout, const array_t& outputs,
    const std::optional<object_t>& options) NOEXCEPT
{
    if (stopped(ec)) return false;

    if (outputs.empty())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    auto mempool_only = false;
    auto return_tx = false;
    for (const auto& [name, value]: options.value_or(object_t{}))
    {
        const auto flag = std::get_if<boolean_t>(&value.value());
        if (is_null(flag))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        if (name == "mempool_only")
            mempool_only = *flag;
        else if (name == "return_spending_tx")
            return_tx = *flag;
        else
        {
            send_error(error::bitcoind::type_error);
            return true;
        }
    }

    chain::points prevouts{};
    prevouts.reserve(outputs.size());
    for (const auto& output: outputs)
    {
        const auto fields = std::get_if<object_t>(&output.value());
        if (is_null(fields) || fields->size() != two)
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        const auto txid = fields->find("txid");
        const auto vout = fields->find("vout");
        if (txid == fields->end() || vout == fields->end() ||
            !std::holds_alternative<string_t>(txid->second.value()) ||
            !std::holds_alternative<number_t>(vout->second.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        hash_digest hash{};
        uint32_t index{};
        if (!decode_hash(hash, std::get<string_t>(txid->second.value())) ||
            !to_integer(index, std::get<number_t>(vout->second.value())))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        prevouts.emplace_back(hash, index);
    }

    PARALLEL(do_get_tx_spending_prevout, std::move(prevouts), mempool_only,
        return_tx, gate());
    return true;
}

// The mempool spender has precedence, as with bitcoind.
void protocol_bitcoind_blockchain::do_get_tx_spending_prevout(
    const chain::points& prevouts, bool mempool_only, bool return_tx,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    array_t results{};
    results.reserve(prevouts.size());
    for (const auto& prevout: prevouts)
    {
        object_t result
        {
            { "txid", encode_hash(prevout.hash()) },
            { "vout", prevout.index() }
        };

        auto confirmed = false;
        auto spender = query.find_confirmable_spender(prevout);
        if (spender.is_terminal() && !mempool_only)
        {
            spender = query.find_confirmed_spender(prevout);
            confirmed = !spender.is_terminal();
        }

        if (!spender.is_terminal())
        {
            const auto tx = query.to_input_tx(spender);
            result["spendingtxid"] = encode_hash(query.get_tx_key(tx));

            if (return_tx)
            {
                const auto spending = query.get_transaction(tx, true);
                if (!spending)
                {
                    POST(fail, error::bitcoind::internal_error, gate);
                    return;
                }

                result["spendingtx"] = encode_base16(spending->to_data(true));
            }

            if (confirmed)
            {
                const auto block = query.find_confirmed_block(tx);
                result["blockhash"] = encode_hash(query.get_header_key(block));
            }
        }

        results.emplace_back(std::move(result));
    }

    POST(complete_value, value_t{ std::move(results) }, gate);
}

bool protocol_bitcoind_blockchain::handle_import_mempool(const code& ec,
    rpc_interface::import_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

// Chase events.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_blockchain::handle_chase(const code&,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (stopped())
        return false;

    switch (node::to_chase(value))
    {
        case node::chase::organized:
        case node::chase::reorganized:
        {
            POST(do_wait_event);
            break;
        }
        default:
            break;
    }

    return true;
}

// Wait machinery (strand).
// ----------------------------------------------------------------------------

void protocol_bitcoind_blockchain::arm_wait(waiter&& waiting,
    double timeout) NOEXCEPT
{
    if (wait_done(waiting))
    {
        send_top();
        return;
    }

    // A zero timeout waits indefinitely.
    uint64_t span{};
    if (!to_integer(span, timeout))
    {
        send_error(error::bitcoind::misc_error);
        return;
    }

    waiting.gate = gate();
    waiting.race = emplace_shared<wait_race>(BIND(complete_wait, _1));
    waiter_ = move_shared(std::move(waiting));

    if (!is_zero(span))
        wait_timer_->start(BIND(handle_wait_timeout, _1, waiter_),
            network::milliseconds(span));
}

void protocol_bitcoind_blockchain::do_wait_event() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!waiter_ || !wait_done(*waiter_))
        return;

    const auto waiting = std::move(waiter_);
    waiting->race->finish(error::success);
}

void protocol_bitcoind_blockchain::handle_wait_timeout(const code& ec,
    const waiter::ptr& waiting) NOEXCEPT
{
    BC_ASSERT(stranded());

    // A stale expiry or cancel follows a completed wait.
    if (waiting != waiter_)
        return;

    waiter_.reset();
    waiting->race->finish(ec);
}

// The race is released without a winner when the channel stops.
void protocol_bitcoind_blockchain::complete_wait(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    wait_timer_->stop();

    if (stopped() || ec == network::error::operation_failed)
        return;

    send_top();
}

bool protocol_bitcoind_blockchain::wait_done(
    const waiter& waiting) const NOEXCEPT
{
    const auto& query = archive();
    switch (waiting.type)
    {
        case wait::new_block:
        case wait::height:
            return query.get_top_confirmed() >= waiting.height;
        case wait::block:
        {
            const auto link = query.to_header(waiting.hash);
            return !link.is_terminal() && query.is_confirmed_block(link);
        }
        default:
            return false;
    }
}

void protocol_bitcoind_blockchain::send_top() NOEXCEPT
{
    size_t top{};
    const auto& query = archive();
    const auto link = query.to_top_confirmed();
    if (!query.get_height(top, link))
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    send_result(object_t
    {
        { "hash", encode_hash(query.get_header_key(link)) },
        { "height", top }
    });
}

// Utilities.
// ----------------------------------------------------------------------------

void protocol_bitcoind_blockchain::fail(const code& ec,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return;

    send_error(ec);
}

void protocol_bitcoind_blockchain::complete_object(object_t& result,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return;

    send_result(std::move(result));
}

void protocol_bitcoind_blockchain::complete_value(value_t& result,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return;

    send_result(std::move(result));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
