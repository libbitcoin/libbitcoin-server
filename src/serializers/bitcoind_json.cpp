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
#include <bitcoin/server/serializers/bitcoind_json.hpp>

#include <unordered_set>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

system::json::value to_btc(uint64_t satoshis) NOEXCEPT
{
    return system::json::decimal{ sign_cast<int64_t>(satoshis),
        btc_decimal_places };
}

// Clamped ratio of validated blocks to chain height.
double progress(size_t blocks, size_t headers) NOEXCEPT
{
    return is_zero(headers) ? 1.0 :
        std::min(1.0, to_floating(blocks) / headers);
}

// A getchainstates entry for candidate or confirmed at the link (top).
network::rpc::object_t chain_states_entry(
    const node::query& query, const database::header_link& link,
    double progress, bool validated) NOEXCEPT
{
    using namespace chain;
    using namespace network::rpc;

    size_t height{};
    const auto header = query.get_header(link);
    if (!header || !query.get_height(height, link))
        return {};

    const auto bits = header->bits();
    return object_t
    {
        // bitcoind "blocks" is a height, not a count.
        { "blocks", to64(height) },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(bits)) },
        { "target", encode_hash(from_uintx(chain::compact::expand(bits))) },
        { "difficulty", header->difficulty() },
        { "verificationprogress", progress },
        { "coins_db_cache_bytes", to64(zero) },
        { "coins_tip_cache_bytes", to64(zero) },
        { "validated", validated }
    };
}

void inject_block_context(system::json::object& out,
    const node::query& query, const system::settings& settings,
    const database::header_link& link, const chain::header& header) NOEXCEPT
{
    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto top = query.get_top_confirmed();
    const auto confirmed = query.is_confirmed_block(link);
    out["height"] = height;

    // bitcoind reports -1 confirmations for a block not on the active chain.
    out["confirmations"] = confirmed ?
        to_signed(add1(floored_subtract(top, height))) : -1;
    out["mediantime"] = median_time(query, settings, link);

    // Cumulative work to this block, big-endian per bitcoind chainwork.
    uint256_t work{};
    if (query.get_branch_work(work, link))
        out["chainwork"] = encode_hash(from_uintx(work));

    if (header.previous_block_hash() != null_hash)
        out["previousblockhash"] = encode_hash(header.previous_block_hash());

    if (confirmed && height < top)
        out["nextblockhash"] = encode_hash(
            query.get_header_key(query.to_confirmed(add1(height))));
}

void inject_tx_context(system::json::object& out,
    const node::query& query, const database::tx_link& link) NOEXCEPT
{
    size_t height{};
    if (!query.get_tx_height(height, link))
    {
        out["confirmations"] = zero;
        return;
    }

    const auto block = query.to_confirmed(height);
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(block);
    out["blockhash"] = encode_hash(query.get_header_key(block));
    out["confirmations"] = add1(floored_subtract(top, height));
    out["in_active_chain"] = true;
    if (header)
    {
        out["blocktime"] = header->timestamp();
        out["time"] = header->timestamp();
    }
}

// The tx must be populated (populate_with_metadata).
void inject_tx_prevouts(system::json::object& out,
    const node::query& query, const chain::transaction& tx, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    size_t height{};
    auto entry = out.at("vin").as_array().begin();
    std::ranges::for_each(*tx.inputs_ptr(), [&](const auto& in) NOEXCEPT
    {
        if (query.get_tx_height(height, in->metadata.parent_tx))
        {
            const auto& put = *in->prevout;
            auto script = to_script_public_key(put.script(), p2kh, p2sh,
                witness, flags);

            system::json::object prevout
            {
                { "generated", in->metadata.coinbase },
                { "height", height },
                { "value", to_btc(put.value()) },
                { "scriptPubKey", std::move(script) }
            };

            entry->as_object()["prevout"] = std::move(prevout);
        }

        ++entry;
    });
}

void inject_script_context(system::json::object& out,
    const chain::script& script, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness) NOEXCEPT
{
    out["desc"] = infer_descriptor(script, p2kh, p2sh, witness);

    // An unaddressable script (including pay-to-public-key) has no address.
    const auto address = to_address(script, p2kh, p2sh, witness);
    if (!address.empty())
        out["address"] = address;
}

void inject_tx_scripts(system::json::object& out,
    const chain::transaction& tx, uint8_t p2kh, uint8_t p2sh,
    const std::string& witness) NOEXCEPT
{
    auto entry = out.at("vout").as_array().begin();
    std::ranges::for_each(*tx.outputs_ptr(), [&](const auto& put) NOEXCEPT
    {
        auto& script = entry->as_object().at("scriptPubKey").as_object();
        inject_script_context(script, put->script(), p2kh, p2sh, witness);
        ++entry;
    });
}

system::json::value to_script_public_key(const chain::script& script,
    uint8_t p2kh, uint8_t p2sh, const std::string& witness,
    uint32_t flags) NOEXCEPT
{
    // Key order differs from bitcoind (asm, desc, hex, address, type).
    auto value = value_from(bitcoind(script, flags));
    inject_script_context(value.as_object(), script, p2kh, p2sh, witness);
    return value;
}

void inject_activity(network::rpc::array_t& out, const chain::block& block,
    size_t height, const std::string& blockhash,
    const std::unordered_set<std::string>& watch, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    const network::rpc::object_t location
    {
        { "blockhash", blockhash },
        { "height", to64(height) }
    };

    for (const auto& tx: *block.transactions_ptr())
        inject_activity(out, *tx, location, watch, p2kh, p2sh, witness, flags);
}

void inject_activity(network::rpc::array_t& out, const chain::transaction& tx,
    const network::rpc::object_t& location,
    const std::unordered_set<std::string>& watch, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    using namespace network::rpc;
    const auto txid = encode_hash(tx.hash(false));
    uint32_t index{};
    for (const auto& put: *tx.outputs_ptr())
    {
        const auto script = encode_base16(put->script().to_data(false));
        if (watch.contains(script))
        {
            auto output = to_script_public_key(put->script(), p2kh, p2sh,
                witness, flags);

            object_t entry
            {
                { "type", std::string{ "receive" } },
                { "amount", to_btc(put->value()) },
                { "txid", txid },
                { "vout", index },
                { "output_spk", std::move(output) }
            };

            entry.insert(location.cbegin(), location.cend());
            out.emplace_back(std::move(entry));
        }

        ++index;
    }

    if (tx.is_coinbase())
        return;

    uint32_t spend{};
    for (const auto& in: *tx.inputs_ptr())
    {
        const auto& prevout = *in->prevout;
        const auto script = encode_base16(prevout.script().to_data(false));
        if (watch.contains(script))
        {
            auto output = to_script_public_key(prevout.script(), p2kh, p2sh,
                witness, flags);

            object_t entry
            {
                { "type", std::string{ "spend" } },
                { "amount", to_btc(prevout.value()) },
                { "spend_txid", txid },
                { "spend_vin", spend },
                { "prevout_txid", encode_hash(in->point().hash()) },
                { "prevout_vout", in->point().index() },
                { "prevout_spk", std::move(output) }
            };

            entry.insert(location.cbegin(), location.cend());
            out.emplace_back(std::move(entry));
        }

        ++spend;
    }
}

std::string chain_name(const system::settings& settings) NOEXCEPT
{
    switch (settings.genesis_network)
    {
        case chain::selection::mainnet: return "main";
        case chain::selection::testnet3: return "test";
        case chain::selection::testnet4: return "testnet4";
        case chain::selection::regtest: return "regtest";
        case chain::selection::signet: return "signet";
        case chain::selection::testnet5: return "testnet5";
        case chain::selection::none:
        default: return "unknown";
    }
}

// Shared by bitcoind blockchain subgroup and btcd endpoint, which augments
// result with bip9_softforks (required by lnd).
bool chain_info(network::rpc::object_t& out,
    const node::query& query, const system::settings& settings,
    bool pruned, bool current) NOEXCEPT
{
    size_t blocks{};
    const auto headers = query.get_top_candidate();
    const auto link = query.to_top_confirmed();
    const auto header = query.get_header(link);

    uint256_t work{};
    database::context context{};
    if (!header || !query.get_height(blocks, link) ||
        !query.get_branch_work(work, link) || !query.get_context(context, link))
        return false;

    const auto bits = header->bits();
    out = network::rpc::object_t
    {
        // bitcoind "blocks" is a height, not a count.
        { "chain", chain_name(settings) },
        { "blocks", to64(blocks) },
        { "headers", to64(headers) },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(bits)) },
        { "target", encode_hash(from_uintx(chain::compact::expand(bits))) },
        { "difficulty", header->difficulty() },
        { "time", header->timestamp() },
        { "mediantime", median_time(query, settings, link) },
        { "verificationprogress", progress(blocks, headers) },
        { "initialblockdownload", !current },
        { "chainwork", encode_hash(from_uintx(work)) },
        { "size_on_disk", to64(query.store_size()) },
        { "pruned", pruned },
        { "warnings", network::rpc::array_t{} }
    };

    // A deployment is active if flagged in the top confirmed header context.
    network::rpc::object_t soft_forks{};
    const auto add_fork = [&](const std::string& name, int32_t bit,
        chain::flags first, chain::flags second) NOEXCEPT
    {
        if (!context.is_enabled(first) || !context.is_enabled(second))
            return;

        network::rpc::object_t fork
        {
            { "status", std::string{ "active" } },
            { "bit", bit },
            { "startTime", -1 },
            { "timeout", -1 },
            { "min_activation_height", 0 }
        };

        size_t since{};
        if (query.get_activation(since, first))
            fork.emplace("since", to64(since));

        soft_forks.emplace(name, std::move(fork));
    };

    using namespace chain;
    add_fork("csv", 0, flags::bip68_rule, flags::bip112_rule);
    add_fork("segwit", 1, flags::bip141_rule, flags::bip143_rule);
    add_fork("taproot", 2, flags::bip341_rule, flags::bip342_rule);
    out.emplace("bip9_softforks", std::move(soft_forks));

    return true;
}

// The entry height is that of the chain top, below the pooled context height.
bool mempool_entry(system::json::object& out, const node::query& query,
    const database::tx_link& link) NOEXCEPT
{
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
        system::json::array txids{};
        txids.reserve(links.size());
        for (const auto& fk: links)
            txids.emplace_back(encode_hash(query.get_tx_key(fk)));

        return txids;
    };

    const system::json::object fees
    {
        { "base", to_btc(rate.fee) },
        { "modified", to_btc(rate.fee) },
        { "ancestor", to_btc(ancestors.fee) },
        { "descendant", to_btc(descendants.fee) }
    };

    out = system::json::object
    {
        { "vsize", rate.bytes },
        { "weight", chain::weighted_size(light, heavy) },
        { "time", time },
        { "height", sub1(rate.pooled_height) },
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

bool mempool_txs(system::json::value& out, const node::query& query,
    const database::tx_links& links, bool verbose) NOEXCEPT
{
    if (verbose)
    {
        system::json::object entries{};
        for (const auto& link: links)
        {
            system::json::object entry{};
            if (!mempool_entry(entry, query, link))
                return false;

            entries[encode_hash(query.get_tx_key(link))] = std::move(entry);
        }

        out = std::move(entries);
        return true;
    }

    system::json::array txids{};
    txids.reserve(links.size());
    for (const auto& link: links)
        txids.emplace_back(encode_hash(query.get_tx_key(link)));

    out = std::move(txids);
    return true;
}

// The zmq mempool sequence is local to each channel, which starts it at one.
bool raw_mempool(system::json::value& out, const node::query& query,
    const database::tx_links& links, bool verbose, bool sequence) NOEXCEPT
{
    system::json::value txs{};
    if (!mempool_txs(txs, query, links, verbose))
        return false;

    if (verbose || !sequence)
    {
        out = std::move(txs);
        return true;
    }

    out = system::json::object
    {
        { "txids", std::move(txs) },
        { "mempool_sequence", 0 }
    };

    return true;
}

// Pool is stored, so usage is size of its tables, maximum is space available to
// store (shared with all tables and volume).
bool mempool_info(system::json::object& out, const node::query& query,
    const database::fee_rates& rates, const std::filesystem::path& store,
    uint64_t minimum, uint64_t relay, uint64_t increment) NOEXCEPT
{
    uint64_t space{};
    if (!database::file::space(space, store))
        return false;

    size_t bytes{};
    uint64_t fee{};
    for (const auto& rate: rates)
    {
        bytes = ceilinged_add(bytes, rate.bytes);
        fee = ceilinged_add(fee, rate.fee);
    }

    const auto indexes = ceilinged_add(query.spends_size(), query.wtxid_size());
    const auto usage = ceilinged_add(query.pool_size(), indexes);
    out = system::json::object
    {
        { "loaded", true },
        { "size", rates.size() },
        { "bytes", bytes },
        { "usage", usage },
        { "total_fee", to_btc(fee) },
        { "maxmempool", space },
        { "mempoolminfee", to_btc(minimum) },
        { "minrelaytxfee", to_btc(relay) },
        { "incrementalrelayfee", to_btc(increment) },
        { "unbroadcastcount", 0 },
        { "fullrbf", false }
    };

    return true;
}

bool package_result(system::json::object& out, const node::query& query,
    const code& ec, size_t index,
    const chain::transaction_cptrs& txs) NOEXCEPT
{
    const auto failed = ec && ec != node::error::duplicate_transaction;
    system::json::object results{};
    for (size_t position{}; position < txs.size(); ++position)
    {
        const auto& tx = *txs.at(position);
        const auto wtxid = encode_hash(tx.hash(true));
        system::json::object result{ { "txid", encode_hash(tx.hash(false)) } };
        if (failed)
        {
            result["error"] = position == index ? error::bitcoind::reject(ec) :
                std::string{ "package-not-validated" };
            results[wtxid] = std::move(result);
            continue;
        }

        database::fee_rate rate{};
        if (!query.get_tx_fees(rate, query.to_tx(tx.hash(false))))
            return false;

        const auto bytes = possible_wide_cast<uint64_t>(rate.bytes);
        const auto feerate = rate.fee * 1'000u / bytes;
        result["vsize_adjusted"] = rate.bytes;
        result["vsize"] = rate.bytes;
        result["vsize_bip141"] = tx.virtual_size();
        result["fees"] = system::json::object
        {
            { "base", to_btc(rate.fee) },
            { "effective-feerate", to_btc(feerate) },
            { "effective-includes", system::json::array{ wtxid } }
        };

        results[wtxid] = std::move(result);
    }

    out = system::json::object
    {
        { "package_msg", failed ? "transaction failed" : "success" },
        { "tx-results", std::move(results) },
        { "replaced-transactions", system::json::array{} }
    };

    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
