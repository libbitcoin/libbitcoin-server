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
#include <bitcoin/server/protocols/protocol_bitcoind.hpp>

#include <algorithm>
#include <vector>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

// Clamped ratio of validated blocks to chain height.
double protocol_bitcoind::progress(size_t blocks, size_t headers) NOEXCEPT
{
    return is_zero(headers) ? 1.0 :
        std::min(1.0, to_floating(blocks) / headers);
}

// bitcoind's mtp window includes the block: the child's stored context mtp.
uint32_t protocol_bitcoind::median_time(const node::query& query,
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
network::rpc::object_t protocol_bitcoind::chain_states_entry(
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

void protocol_bitcoind::inject_block_context(boost::json::object& out,
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

void protocol_bitcoind::inject_tx_context(boost::json::object& out,
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
void protocol_bitcoind::inject_tx_prevouts(boost::json::object& out,
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

boost::json::object protocol_bitcoind::header_to_bitcoind(
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

std::string protocol_bitcoind::chain_name(const node::query& query) NOEXCEPT
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
bool protocol_bitcoind::chain_info(network::rpc::object_t& out,
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
// The createmultisig result, empty if a key is invalid or the p2sh embedded
// script exceeds one push element. An uncompressed key downgrades a segwit
// address type to legacy with a warning (as bitcoind).
network::rpc::object_t protocol_bitcoind::create_multisig(uint8_t required,
    const network::rpc::array_t& keys,
    const std::string& address_type) const NOEXCEPT
{
    using namespace chain;
    using namespace wallet;
    using namespace network::rpc;

    std::string list{};
    data_stack points{};
    auto compressed = true;
    points.reserve(keys.size());
    for (const auto& key: keys)
    {
        data_chunk point{};
        if (!std::holds_alternative<string_t>(key.value()) ||
            !decode_base16(point, std::get<string_t>(key.value())) ||
            !is_public_key(point))
            return {};

        compressed &= is_compressed_key(point);

        // Preceded by required in the descriptor list: multi(N,key,...).
        list += "," + encode_base16(point);
        points.push_back(std::move(point));
    }

    const script multisig{ script::to_pay_multisig_pattern(required, points) };
    const auto embedded = multisig.to_data(false);
    const auto type = compressed ? address_type : "legacy";
    if (type != "bech32" && embedded.size() > max_push_data_size)
        return {};

    std::string address{};
    auto body = "multi(" + std::to_string(required) + list + ")";
    if (type == "legacy")
    {
        address = payment_address{ multisig, p2sh_ }.encoded();
        body = "sh(" + body + ")";
    }
    else if (type == "bech32")
    {
        address = witness_address{ multisig, witness_ }.encoded();
        body = "wsh(" + body + ")";
    }
    else
    {
        const auto hash = sha256_hash(embedded);
        const script wsh{ script::to_pay_witness_pattern(0, hash) };
        address = payment_address{ wsh, p2sh_ }.encoded();
        body = "sh(wsh(" + body + "))";
    }

    object_t result
    {
        { "address", address },
        { "redeemScript", encode_base16(embedded) },
        { "descriptor", body + "#" + descriptor_checksum(body) }
    };

    if (type != address_type)
    {
        // This is ridiculous.
        result.emplace("warnings", array_t{ std::string{ "Unable to make "
            "chosen address type, please ensure no uncompressed public keys "
            "are present." } });
    }

    return result;
}

// The address of a singular output script (empty if unaddressable).
std::string protocol_bitcoind::to_address(
    const chain::script& script) const NOEXCEPT
{
    using namespace wallet;

    const auto& ops = script.ops();
    if (chain::script::is_pay_witness_pattern(ops))
    {
        // TODO: this should be an extractor (don't parse scripts).
        const auto code = ops.front().code();
        const auto& program = ops.at(1).data();
        const auto version = chain::operation::opcode_to_nonnegative(code);
        return witness_address{ program, version, witness_ }.encoded();
    }

    const auto pay = payment_address::extract_output(script, p2kh_, p2sh_);
    return pay ? pay.encoded() : std::string{};
}

// Inferred where a pattern is expressible, otherwise raw.
std::string protocol_bitcoind::infer_descriptor(
    const chain::script& script) const NOEXCEPT
{
    std::string body{};

    const auto& ops = script.ops();
    if (chain::script::is_pay_public_key_pattern(ops))
    {
        // TODO: this should be an extractor (don't parse scripts).
        body = "pk(" + encode_base16(ops.front().data()) + ")";
    }
    else if (chain::script::is_pay_multisig_pattern(ops))
    {
        // TODO: this should be an extractor (don't parse scripts).
        using namespace chain;
        const auto code = ops.front().code();
        body = "multi(" + std::to_string(operation::opcode_to_positive(code));
        for (auto op = std::next(ops.begin());
            op != std::prev(ops.end(), 2); ++op)
            body += "," + encode_base16(op->data());

        body += ")";
    }
    else
    {
        const auto address = to_address(script);
        body = address.empty() ?
            "raw(" + encode_base16(script.to_data(false)) + ")" :
            "addr(" + address + ")";
    }

    return body + "#" + descriptor_checksum(body);
}

} // namespace server
} // namespace libbitcoin
