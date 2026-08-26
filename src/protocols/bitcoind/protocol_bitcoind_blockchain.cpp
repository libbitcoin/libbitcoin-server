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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

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

static bool expand_scan_object(chain::scripts& out,
    const value_t& item) NOEXCEPT;

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
    SUBSCRIBE_BITCOIND(handle_precious_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_blocks, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_BITCOIND(handle_wait_for_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_wait_for_block_height, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_wait_for_new_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mempool_ancestors, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_cluster, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_descendants, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_entry, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_mempool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_spending_prevout, _1, _2);
    SUBSCRIBE_BITCOIND(handle_import_mempool, _1, _2);
    subscribe_chase(BIND(handle_chase, _1, _2, _3));
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

void protocol_bitcoind_blockchain::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    unsubscribe_chase();
    wait_timer_->stop();
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
    const auto block = query.get_block(link, witness);
    if (!block)
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash,
            blockhash.size());
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

    if (level == block_verbosity::prevouts &&
        query.populate_without_metadata(*block))
    {
        auto entry = model.as_object().at("tx").as_array().begin();
        std::ranges::for_each(*block->transactions_ptr(),
            [&](const auto& tx) NOEXCEPT
        {
            if (!tx->is_coinbase())
            {
                inject_tx_prevouts(entry->as_object(), query, *tx);
                entry->as_object()["fee"] =
                    tx->fee() / to_floating(chain::satoshi_per_bitcoin);
            }

            ++entry;
        });
    }

    send_result(std::move(model), two * block->serialized_size(witness));
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_block_chain_info(const code& ec,
    rpc_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t out{};
    if (!chain_info(out, archive(), node_settings().limited_blocks,
        is_current_chain(true)))
    {
        send_error(error::bitcoind::internal_error);
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
        send_error(error::bitcoind::invalid_address_or_key, blockhash,
            blockhash.size());
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(hash);
    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash,
            blockhash.size());
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

    size_t height{};
    if (!query.get_height(height, link) || query.to_confirmed(height) != link)
    {
        send_error(error::bitcoind::invalid_address_or_key);
        return true;
    }

    // Fees require prevout values, populated from the store.
    const auto block = query.get_block(link, true);
    if (!block || !query.populate_without_metadata(*block))
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    const auto& settings = system_settings();
    const auto subsidy = chain::block::subsidy(height,
        settings.subsidy_interval_blocks, settings.initial_subsidy(),
        settings.forks.bip42);

    auto result = block_stats(*block, height, median_time_past(query, link),
        subsidy);

    // An empty selection returns all statistics, otherwise the named subset.
    if (stats.empty())
    {
        send_result(std::move(result), 1024);
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
        if (it == result.end())
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        selected.emplace(name, it->second);
    }

    send_result(std::move(selected), 1024);
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
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_header(hash);
    }

    size_t height{};
    if (!query.get_height(height, link) || query.to_confirmed(height) != link)
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash,
            blockhash.size());
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& query = archive();
    const auto output_link = query.to_output(hash, index);

    // bitcoind returns json null for a missing or confirmed-spent output; with
    // mempool ignored this matches gettxout's include_mempool=false semantics
    // (is_spent would also count unconfirmed/conflicting/invalid-block spenders).
    if (output_link.is_terminal() || query.is_confirmed_spent(output_link))
    {
        send_result(null_t{}, 42);
        return true;
    }

    const auto output = query.get_output(output_link);
    if (!output)
    {
        send_result(null_t{}, 42);
        return true;
    }

    // Output's tx must exist.
    const auto tx_link = query.to_tx(hash);
    if (tx_link.is_terminal())
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    // Derive header from top for consistent depth result (also cheaper).
    const auto top = query.get_top_confirmed();
    const auto header_link = query.to_confirmed(top);

    // An archived but unconfirmed output is not an unspent coin.
    size_t height{};
    if (!query.get_tx_height(height, tx_link))
    {
        send_result(null_t{}, 42);
        return true;
    }

    const auto depth = add1(floored_subtract(top, height));
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

// bitcoind's utxo set coin serialization (outpoint, height code, value,
// script), the element of both set commitment forms.
static void to_coin_data(data_chunk& out,
    const database::unspent_coin& coin) NOEXCEPT
{
    constexpr auto overhead = hash_size + sizeof(uint32_t) + sizeof(uint32_t) +
        sizeof(uint64_t);

    out.resize(overhead + variable_size(coin.script.size()) +
        coin.script.size());
    stream::out::fast ostream(out);
    write::bytes::fast sink(ostream);
    sink.write_bytes(coin.txid);
    sink.write_4_bytes_little_endian(coin.index);
    sink.write_4_bytes_little_endian(bit_or(shift_left(
        possible_narrow_cast<uint32_t>(coin.height), 1),
        to_int<uint32_t>(coin.coinbase)));
    sink.write_8_bytes_little_endian(coin.value);
    sink.write_variable(coin.script.size());
    sink.write_bytes(coin.script);
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
        // bitcoind restricts specific block queries (coinstatsindex bounds).
        if ((type == set_hash::serialized) || !use_index)
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

        if (!query.get_height(height, link) ||
            query.to_confirmed(height) != link)
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }
    }

    monitor(true);
    PARALLEL(do_get_tx_out_set_info, type, height);
    return true;
}

void protocol_bitcoind_blockchain::do_get_tx_out_set_info(set_hash type,
    size_t height) NOEXCEPT
{
    BC_ASSERT(!stranded());

    object_t result{};
    const auto& query = archive();
    const auto link = query.to_confirmed(height);

    // The pinned ancestry excludes genesis, absent from the set (as bitcoind).
    database::header_links branch{};
    if (!query.get_ancestry(branch, link, height))
    {
        POST(complete_scan, error::bitcoind::internal_error, std::move(result),
            zero);
        return;
    }

    code ec{};
    hash_digest digest{};
    database::unspent_totals totals{};
    if (type == set_hash::none)
    {
        ec = query.get_unspent_totals(stopping_, totals, branch,
            database_settings().turbo);
    }
    else
    {
        // The commitment accumulates each visited coin, and the totals with
        // it, so the totals scan is not repeated for hashed queries.
        muhash3072 muhash{};
        accumulator<sha256> serial{};
        data_chunk element{};
        const auto visit = [&](const database::unspent_coin& coin) NOEXCEPT
        {
            if (coin.first)
                ++totals.transactions;

            to_coin_data(element, coin);
            ++totals.outputs;
            totals.value += coin.value;
            totals.script_bytes += coin.script.size();
            totals.coin_bytes += element.size();
            if (type == set_hash::muhash)
                muhash.insert(element);
            else
                serial.write(element);
        };

        ec = query.get_unspent_coins(stopping_, visit, branch,
            type == set_hash::serialized, database_settings().turbo);
        digest = (type == set_hash::muhash) ? muhash.flush() :
            serial.double_flush();
    }

    if (ec)
    {
        POST(complete_scan, error::bitcoind::internal_error,
            std::move(result), zero);
        return;
    }

    // A reorganization across the pinned top voids the scan.
    if (!query.is_confirmed_block(link))
    {
        POST(complete_scan, error::bitcoind::internal_error, std::move(result),
            zero);
        return;
    }

    result = object_t
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

    if (type == set_hash::serialized)
        result.emplace("hash_serialized_3", encode_hash(digest));

    if (type == set_hash::muhash)
        result.emplace("muhash", encode_hash(digest));

    POST(complete_scan, code{},
        std::move(result), 512);
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
        send_result(null_t{}, 8);
        return true;
    }

    if (action == "abort")
    {
        send_result(value{ false }, 8);
        return true;
    }

    if (action != "start")
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    monitor(true);
    PARALLEL(do_scan_tx_out_set, std::make_shared<array_t>(scanobjects));
    return true;
}

void protocol_bitcoind_blockchain::do_scan_tx_out_set(
    const std::shared_ptr<array_t>& objects) NOEXCEPT
{
    BC_ASSERT(!stranded());

    object_t result{};
    chain::scripts scripts{};
    for (const auto& item: *objects)
    {
        if (!expand_scan_object(scripts, item))
        {
            POST(complete_scan, error::bitcoind::invalid_address_or_key,
                std::move(result), zero);
            return;
        }
    }

    // Needle identity is the script hash (the address index key). The set
    // excludes unspendable outputs, so those needles are dropped (as
    // bitcoind), and needles deduplicate repeated expressions.
    struct needle { std::string hex; std::string desc; };
    std::unordered_map<hash_digest, needle> needles{};
    for (const auto& script: scripts)
    {
        if (script.is_unspendable())
            continue;

        const auto data = script.to_data(false);
        needles.emplace(sha256_hash(data),
            needle{ encode_base16(data), infer_descriptor(script) });
    }

    struct match
    {
        hash_digest txid;
        uint32_t index;
        uint64_t value;
        size_t height;
        bool coinbase;
        const needle* found;
    };

    code ec{};
    uint64_t txouts{};
    std_vector<match> matches{};
    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);
    const auto bip30 = query.envelope().forks.bip30;

    if (query.address_enabled())
    {
        // Indexed candidates avoid the full scan, so the scanned coin count
        // reflects only examined candidates (bitcoind scans the whole set).
        for (const auto& [key, item]: needles)
        {
            database::unspents outs{};
            ec = query.get_confirmed_unspent(stopping_, outs, key,
                database_settings().turbo);
            if (ec)
                break;

            for (const auto& utxo: outs)
            {
                // The genesis output is excluded from the set (as bitcoind).
                if (is_zero(utxo.height) && is_zero(utxo.position))
                    continue;

                ++txouts;
                matches.emplace_back(utxo.out.point().hash(),
                    utxo.out.point().index(), utxo.out.value(), utxo.height,
                    is_zero(utxo.position), &item);
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
            const auto visit = [&](const database::unspent_coin& coin) NOEXCEPT
            {
                ++txouts;
                const auto it = needles.find(accumulator<sha256>::hash(
                    coin.script));
                if (it != needles.end())
                    matches.emplace_back(coin.txid, coin.index, coin.value,
                        coin.height, coin.coinbase, &it->second);
            };

            ec = query.get_unspent_coins(stopping_, visit, branch, false,
                database_settings().turbo);
        }
    }

    if (ec)
    {
        POST(complete_scan, error::bitcoind::internal_error,
            std::move(result), zero);
        return;
    }

    // A reorganization across the pinned top voids the scan.
    if (!query.is_confirmed_block(link))
    {
        POST(complete_scan, error::bitcoind::internal_error,
            std::move(result), zero);
        return;
    }

    // Report in canonical (txid, index) order (as bitcoind).
    std::sort(matches.begin(), matches.end(),
        [](const auto& left, const auto& right) NOEXCEPT
        {
            return (left.txid == right.txid) ? (left.index < right.index) :
                (left.txid < right.txid);
        });

    uint64_t amount{};
    array_t unspents{};
    for (auto& item: matches)
    {
        // bitcoind retains duplicated coinbases at the overwriting heights.
        if (bip30 && item.coinbase)
            item.height = (item.height == 91812) ? 91842 :
                (item.height == 91722) ? 91880 : item.height;

        unspents.emplace_back(object_t
        {
            { "txid", encode_hash(item.txid) },
            { "vout", item.index },
            { "scriptPubKey", item.found->hex },
            { "desc", item.found->desc },
            { "amount", to_floating(item.value) /
                chain::satoshi_per_bitcoin },
            { "coinbase", item.coinbase },
            { "height", item.height },
            { "blockhash", encode_hash(query.get_header_key(
                query.to_confirmed(item.height))) },
            { "confirmations", add1(floored_subtract(top, item.height)) }
        });

        amount += item.value;
    }

    const auto size = add1(unspents.size()) * 384u;
    result = object_t
    {
        { "success", true },
        { "txouts", txouts },
        { "height", top },
        { "bestblock", encode_hash(query.get_header_key(link)) },
        { "unspents", std::move(unspents) },
        { "total_amount", to_floating(amount) / chain::satoshi_per_bitcoin }
    };

    POST(complete_scan, code{},
        std::move(result), size);
}

void protocol_bitcoind_blockchain::complete_scan(const code& ec,
    object_t& result, size_t size) NOEXCEPT
{
    BC_ASSERT(stranded());
    monitor(false);
    if (stopped())
        return;

    if (ec)
        send_error(ec);
    else
        send_result(std::move(result), size);
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
    auto link = query.find_confirmed_block(*targets.begin());

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

    if (to_unsigned(std::count(match.begin(), match.end(), true)) !=
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
        send_result(array_t{}, 2);
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

    send_result(std::move(result), 32 * add1(matched.size()));
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

    const auto& query = archive();
    const auto candidate = query.get_top_candidate();

    auto link = query.to_confirmed(query.get_top_confirmed());
    auto entry = chain_states_entry(query, link, 1.0, true);
    if (entry.empty())
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    array_t states{ std::move(entry) };

    if (!query.is_coalesced())
    {
        // The candidate chain is validated to the fork point (confirmed) plus
        // the contiguously validated span above it. This does not imply
        // confirmability, which is not determined until blocks are
        // reorganized into the confirmed chain.
        size_t fork{};
        const auto span = query.get_validated_fork(fork,
            system_settings().top_checkpoint().height());
        const auto validated = progress(fork + span.size(), candidate);

        link = query.to_candidate(candidate);
        entry = chain_states_entry(query, link, validated, false);
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
    }, 512);
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
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        link = query.to_header(hash);
    }

    size_t height{};
    if (!query.get_height(height, link))
    {
        send_error(error::bitcoind::invalid_address_or_key, blockhash,
            blockhash.size());
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

// The mempool option is meaningless here (no mempool), always applied false.
bool protocol_bitcoind_blockchain::handle_get_descriptor_activity(
    const code& ec, rpc_interface::get_descriptor_activity,
    const array_t& blockhashes, const array_t& scanobjects, bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::scripts derived{};
    for (const auto& item: scanobjects)
    {
        if (!expand_scan_object(derived, item))
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }
    }

    std::unordered_set<std::string> watch{};
    for (const auto& script: derived)
        watch.insert(encode_base16(script.to_data(false)));

    const auto& query = archive();
    array_t activity{};
    for (const auto& item: blockhashes)
    {
        hash_digest hash{};
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        constexpr auto witness = true;
        const auto link = query.to_header(hash);
        const auto block = query.get_block(link, witness);
        size_t height{};
        if (!block || !query.get_height(height, link))
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }

        // Confirmed spends are unconditional (as bitcoind, from undo data).
        const auto encoded = encode_hash(hash);
        if (!query.populate_without_metadata(*block))
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }

        for (const auto& tx: *block->transactions_ptr())
        {
            const auto txid = encode_hash(tx->hash(false));
            uint32_t index{};
            for (const auto& out: *tx->outputs_ptr())
            {
                const auto script = encode_base16(
                    out->script().to_data(false));
                if (watch.contains(script))
                {
                    activity.emplace_back(object_t
                    {
                        { "type", std::string{ "receive" } },
                        { "amount", out->value() /
                            to_floating(chain::satoshi_per_bitcoin) },
                        { "blockhash", encoded },
                        { "height", height },
                        { "txid", txid },
                        { "vout", index },
                        { "output_spk", value_from(bitcoind(out->script())) }
                    });
                }

                ++index;
            }

            if (tx->is_coinbase())
                continue;

            uint32_t spend{};
            for (const auto& in: *tx->inputs_ptr())
            {
                const auto& prevout = *in->prevout;
                const auto script = encode_base16(
                    prevout.script().to_data(false));
                if (watch.contains(script))
                {
                    activity.emplace_back(object_t
                    {
                        { "type", std::string{ "spend" } },
                        { "amount", prevout.value() /
                            to_floating(chain::satoshi_per_bitcoin) },
                        { "blockhash", encoded },
                        { "height", height },
                        { "spend_txid", txid },
                        { "spend_vin", spend },
                        { "prevout_txid", encode_hash(in->point().hash()) },
                        { "prevout_vout", in->point().index() },
                        { "prevout_spk", value_from(bitcoind(
                            prevout.script())) }
                    });
                }

                ++spend;
            }
        }
    }

    const auto size = 256 * activity.size();
    send_result(object_t{ { "activity", std::move(activity) } }, size);
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
        send_error(error::bitcoind::internal_error);
        return true;
    }

    send_result(header->difficulty(), 20);
    return true;
}

bool protocol_bitcoind_blockchain::handle_precious_block(const code& ec,
    rpc_interface::precious_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

// A scan object is a descriptor string or { "desc", "range" } object.
static bool expand_scan_object(chain::scripts& out,
    const value_t& item) NOEXCEPT
{
    std::string expression{};
    uint32_t begin{};
    uint32_t end{};

    // bitcoind's default and maximum ranges for ranged descriptors.
    constexpr uint32_t default_range = 1'000;
    constexpr uint32_t maximum_range = 1'000'000;

    if (std::holds_alternative<string_t>(item.value()))
    {
        expression = std::get<string_t>(item.value());
        end = default_range;
    }
    else if (std::holds_alternative<object_t>(item.value()))
    {
        const auto& fields = std::get<object_t>(item.value());
        const auto desc = fields.find("desc");
        if (desc == fields.end() ||
            !std::holds_alternative<string_t>(desc->second.value()))
            return false;

        expression = std::get<string_t>(desc->second.value());
        end = default_range;
        const auto range = fields.find("range");
        if (range != fields.end())
        {
            const auto& value = range->second.value();
            if (std::holds_alternative<number_t>(value))
            {
                if (!to_integer(end, std::get<number_t>(value)))
                    return false;
            }
            else if (std::holds_alternative<array_t>(value))
            {
                const auto& pair = std::get<array_t>(value);
                if (pair.size() != 2u ||
                    !std::holds_alternative<number_t>(pair.front().value()) ||
                    !std::holds_alternative<number_t>(pair.back().value()) ||
                    !to_integer(begin,
                        std::get<number_t>(pair.front().value())) ||
                    !to_integer(end,
                        std::get<number_t>(pair.back().value())) ||
                    end < begin)
                    return false;
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    const wallet::descriptor parsed{ expression };
    if (!parsed || to_bool(shift_right(end, 31u)) ||
        floored_subtract(end, begin) >= maximum_range)
        return false;

    if (!parsed.ranged())
        end = begin;

    for (auto index = begin; index <= end; ++index)
    {
        const auto derived = parsed.scripts(index);
        if (derived.empty())
            return false;

        out.insert(out.end(), derived.begin(), derived.end());
    }

    return true;
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
        if (!expand_scan_object(scripts, item))
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }
    }

    array_t relevant{};
    for (auto height = from; height <= to; ++height)
    {
        const auto link = query.to_confirmed(height);
        const auto hash = query.get_header_key(link);
        neutrino::block_filter filter{ hash, {} };
        if (!query.get_filter_body(filter.filter, link))
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
    }, 1024);
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

    if (!decode_hash(wait_hash_, blockhash))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    wait_ = wait::block;
    arm_wait(timeout);
    return true;
}

bool protocol_bitcoind_blockchain::handle_wait_for_block_height(const code& ec,
    rpc_interface::wait_for_block_height, const double height,
    double timeout) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!to_integer(wait_height_, height))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    wait_ = wait::height;
    arm_wait(timeout);
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
        send_tip();
        return true;
    }

    wait_ = wait::new_block;
    wait_height_ = add1(archive().get_top_confirmed());
    arm_wait(timeout);
    return true;
}

// Wait machinery (strand).
// ----------------------------------------------------------------------------

bool protocol_bitcoind_blockchain::wait_done() const NOEXCEPT
{
    const auto& query = archive();
    switch (wait_)
    {
        case wait::new_block:
        case wait::height:
            return query.get_top_confirmed() >= wait_height_;
        case wait::block:
        {
            const auto link = query.to_header(wait_hash_);
            return !link.is_terminal() && query.is_confirmed_block(link);
        }
        default:
            return false;
    }
}

void protocol_bitcoind_blockchain::send_tip() NOEXCEPT
{
    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    send_result(object_t
    {
        { "hash", encode_hash(query.get_header_key(query.to_confirmed(top))) },
        { "height", top }
    }, 128);
}

void protocol_bitcoind_blockchain::arm_wait(double timeout) NOEXCEPT
{
    if (wait_done())
    {
        wait_ = wait::none;
        send_tip();
        return;
    }

    // A zero timeout waits indefinitely (as bitcoind).
    uint64_t span{};
    if (!to_integer(span, timeout))
    {
        wait_ = wait::none;
        send_error(error::bitcoind::misc_error);
        return;
    }

    if (!is_zero(span))
        wait_timer_->start(BIND(handle_wait_timeout, _1),
            network::milliseconds(span));
}

void protocol_bitcoind_blockchain::do_wait_event() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (wait_ == wait::none || !wait_done())
        return;

    wait_ = wait::none;
    wait_timer_->stop();
    send_tip();
}

void protocol_bitcoind_blockchain::handle_wait_timeout(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped() || ec == network::error::operation_canceled ||
        wait_ == wait::none)
        return;

    wait_ = wait::none;
    send_tip();
}

// Chase events.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_blockchain::handle_chase(const code&,
    node::chase event_, node::event_value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (stopped())
        return false;

    switch (event_)
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

bool protocol_bitcoind_blockchain::handle_get_mempool_ancestors(const code& ec,
    rpc_interface::get_mempool_ancestors) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_cluster(const code& ec,
    rpc_interface::get_mempool_cluster) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_descendants(const code& ec,
    rpc_interface::get_mempool_descendants) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_entry(const code& ec,
    rpc_interface::get_mempool_entry) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_mempool_info(const code& ec,
    rpc_interface::get_mempool_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_raw_mempool(const code& ec,
    rpc_interface::get_raw_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_get_tx_spending_prevout(const code& ec,
    rpc_interface::get_tx_spending_prevout) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_blockchain::handle_import_mempool(const code& ec,
    rpc_interface::import_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
