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

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Clamped ratio of validated blocks to chain height.
double progress(size_t blocks, size_t headers) NOEXCEPT
{
    return is_zero(headers) ? 1.0 :
        std::min(1.0, to_floating(blocks) / headers);
}

// bitcoind's mtp window includes the block: the child's stored context mtp.
uint32_t median_time(const node::query& query,
    const system::settings& settings,
    const database::header_link& link) NOEXCEPT
{
    database::context ctx{};
    if (query.get_context(ctx, query.to_confirmed_child(link)))
        return ctx.mtp;

    // The top block has no child, its promoted chain state carries the value.
    const auto key = query.get_header_key(link);
    const auto state = query.get_confirmed_chain_state(settings, key);
    if (!state)
        return 0_u32;

    return chain::chain_state{ *state, settings }.context().median_time_past;
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
        // bitcoind OB1 error ("blocks" wants height).
        { "blocks", height },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(bits)) },
        { "target", encode_hash(from_uintx(compact::expand(bits))) },
        { "difficulty", header->difficulty() },
        { "verificationprogress", progress },
        { "coins_db_cache_bytes", zero },
        { "coins_tip_cache_bytes", zero },
        { "validated", validated }
    };
}

void inject_block_context(boost::json::object& out,
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

void inject_tx_context(boost::json::object& out,
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
void inject_tx_prevouts(boost::json::object& out,
    const node::query& query, const chain::transaction& tx) NOEXCEPT
{
    size_t height{};
    auto entry = out.at("vin").as_array().begin();
    std::ranges::for_each(*tx.inputs_ptr(), [&](const auto& in) NOEXCEPT
    {
        if (query.get_tx_height(height, in->metadata.parent_tx))
        {
            auto put = value_from(bitcoind(*in->prevout)).as_object();
            boost::json::object prevout
            {
                { "generated", in->metadata.coinbase },
                { "height", height },
                { "value", put.at("value") },
                { "scriptPubKey", std::move(put.at("scriptPubKey")) }
            };

            entry->as_object()["prevout"] = std::move(prevout);
        }

        ++entry;
    });
}

void inject_activity(network::rpc::array_t& out, const chain::block& block,
    size_t height, const std::string& blockhash,
    const std::unordered_set<std::string>& watch) NOEXCEPT
{
    using namespace network::rpc;
    for (const auto& tx: *block.transactions_ptr())
    {
        const auto txid = encode_hash(tx->hash(false));
        uint32_t index{};
        for (const auto& put: *tx->outputs_ptr())
        {
            const auto script = encode_base16(put->script().to_data(false));
            if (watch.contains(script))
            {
                out.emplace_back(object_t
                {
                    { "type", std::string{ "receive" } },
                    { "amount", put->value() /
                        to_floating(chain::satoshi_per_bitcoin) },
                    { "blockhash", blockhash },
                    { "height", height },
                    { "txid", txid },
                    { "vout", index },
                    { "output_spk", value_from(bitcoind(put->script())) }
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
            const auto script = encode_base16(prevout.script().to_data(false));
            if (watch.contains(script))
            {
                out.emplace_back(object_t
                {
                    { "type", std::string{ "spend" } },
                    { "amount", prevout.value() /
                        to_floating(chain::satoshi_per_bitcoin) },
                    { "blockhash", blockhash },
                    { "height", height },
                    { "spend_txid", txid },
                    { "spend_vin", spend },
                    { "prevout_txid", encode_hash(in->point().hash()) },
                    { "prevout_vout", in->point().index() },
                    { "prevout_spk", value_from(bitcoind(prevout.script())) }
                });
            }

            ++spend;
        }
    }
}

std::string to_address(const chain::script& script, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    using namespace wallet;

    const auto version = script.version_value();
    if (version != to_value(chain::script_version::unversioned))
        return witness_address{ *script.witness_program(), version,
            witness }.encoded();

    const auto pay = payment_address::extract_output(script, p2kh, p2sh);
    return pay ? pay.encoded() : std::string{};
}

boost::json::object header_to_bitcoind(
    const chain::header& header) NOEXCEPT
{
    return boost::json::object
    {
        { "hash", encode_hash(header.hash()) },
        { "version", header.version() },
        { "versionHex", encode_base16(to_big_endian(header.version())) },
        { "merkleroot", encode_hash(header.merkle_root()) },
        { "time", header.timestamp() },
        { "nonce", header.nonce() },
        { "bits", encode_base16(to_big_endian(header.bits())) },
        { "target", encode_hash(from_uintx(chain::compact::expand(header.bits()))) },
        { "difficulty", header.difficulty() }
    };
}

std::string chain_name(const node::query& query) NOEXCEPT
{
    const auto genesis = query.get_header_key(query.to_confirmed(zero));

    // TODO: create signet chain selector.
    // See libbitcoin-system#1908.
    using selection = chain::selection;
    constexpr auto signet = base16_hash(
        "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6");
    static const std::vector<std::pair<hash_digest, std::string>> networks
    {
        { system::settings{ selection::mainnet }.genesis_block.hash(), "main" },
        { system::settings{ selection::testnet3 }.genesis_block.hash(), "test" },
        { system::settings{ selection::regtest }.genesis_block.hash(), "regtest" },
        { signet, "signet" }
    };

    for (const auto& [hash, name]: networks)
        if (hash == genesis)
            return name;

    return "unknown";
}

// Shared by the bitcoind blockchain subgroup and the btcd endpoint, which
// augments the result with bip9_softforks (required by lnd).
bool chain_info(network::rpc::object_t& out,
    const node::query& query, const system::settings& settings,
    bool pruned, bool current) NOEXCEPT
{
    const auto blocks = query.get_top_confirmed();
    const auto headers = query.get_top_candidate();
    const auto link = query.to_confirmed(blocks);
    const auto header = query.get_header(link);

    uint256_t work{};
    if (!header || !query.get_branch_work(work, link))
        return false;

    const auto bits = header->bits();
    out = network::rpc::object_t
    {
        // bitcoind OB1 error ("blocks" wants height).
        { "chain", chain_name(query) },
        { "blocks", blocks },
        { "headers", headers },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(bits)) },
        { "target", encode_hash(from_uintx(chain::compact::expand(bits))) },
        { "difficulty", header->difficulty() },
        { "time", header->timestamp() },
        { "mediantime", median_time(query, settings, link) },
        { "verificationprogress", progress(blocks, headers) },
        { "initialblockdownload", !current },
        { "chainwork", encode_hash(from_uintx(work)) },
        { "size_on_disk", query.store_size() },
        { "pruned", pruned },
        { "warnings", network::rpc::array_t{} }
    };

    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
