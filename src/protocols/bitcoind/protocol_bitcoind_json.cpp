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

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

uint32_t protocol_bitcoind::median_time_past(const node::query& query,
    const database::header_link& link) NOEXCEPT
{
    chain::context ctx{};
    return query.get_context(ctx, link) ? ctx.median_time_past : 0_u32;
}

void protocol_bitcoind::inject_block_context(boost::json::object& out,
    const node::query& query, const database::header_link& link,
    const chain::header& header) NOEXCEPT
{
    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto top = query.get_top_confirmed();
    const auto confirmed = query.is_confirmed_block(link);
    out["height"] = height;
    out["confirmations"] = add1(floored_subtract(top, height));
    out["mediantime"] = median_time_past(query, link);

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
        out["confirmations"] = 0;
        return;
    }

    const auto block = query.to_confirmed(height);
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(block);
    out["in_active_chain"] = true;
    out["blockhash"] = encode_hash(query.get_header_key(block));
    out["confirmations"] = add1(floored_subtract(top, height));
    if (header)
    {
        out["blocktime"] = header->timestamp();
        out["time"] = header->timestamp();
    }
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
        { "difficulty", header.difficulty() }
    };
}

std::string protocol_bitcoind::chain_name(const node::query& query) NOEXCEPT
{
    const auto genesis = query.get_header_key(query.to_confirmed(zero));

    // TODO: create signet chain selector.
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
    const node::query& query, bool pruned, bool current) NOEXCEPT
{
    const auto blocks = query.get_top_confirmed();
    const auto headers = query.get_top_candidate();
    const auto link = query.to_confirmed(blocks);
    const auto header = query.get_header(link);
    if (!header)
        return false;

    // TODO: make utility method and move explanation there.
    // verificationprogress is approximated as confirmed/candidate height, the
    // best available estimate of the chain top during sync (1.0 once current).
    const auto progress = is_zero(headers) ? 1.0 :
        std::min(1.0, to_floating(blocks) / to_floating(headers));

    // TODO: blocks/headers is a misnomer (off-by-one), intended?
    using namespace chain;
    out = network::rpc::object_t
    {
        { "chain", chain_name(query) },
        { "blocks", blocks },
        { "headers", headers },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(header->bits())) },
        { "target", encode_hash(from_uintx(compact::expand(header->bits()))) },
        { "difficulty", header->difficulty() },
        { "time", header->timestamp() },
        { "mediantime", median_time_past(query, link) },
        { "verificationprogress", progress },
        { "initialblockdownload", !current },
        { "pruned", pruned },
        { "warnings", std::string{} }
    };

    return true;
}

} // namespace server
} // namespace libbitcoin
