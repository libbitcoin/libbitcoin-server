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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <algorithm>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

uint32_t protocol_bitcoind_rpc::median_time_past(const node::query& query,
    const database::header_link& link) NOEXCEPT
{
    chain::context ctx{};
    return query.get_context(ctx, link) ? ctx.median_time_past : 0_u32;
}

// verificationprogress is approximated as confirmed/candidate height, the best
// available estimate of the chain tip during sync (1.0 once current).
double protocol_bitcoind_rpc::verification_progress(size_t blocks,
    size_t headers) NOEXCEPT
{
    return is_zero(headers) ? 1.0 :
        std::min(1.0, static_cast<double>(blocks) /
            static_cast<double>(headers));
}

void protocol_bitcoind_rpc::inject_block_context(boost::json::object& out,
    const node::query& query, const database::header_link& link,
    const chain::header& header) NOEXCEPT
{
    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto top = query.get_top_confirmed();
    const auto confirmed = query.is_confirmed_block(link);
    out["height"] = height;

    // bitcoind reports -1 confirmations for a block not on the active chain.
    out["confirmations"] = confirmed ?
        static_cast<int64_t>(add1(floored_subtract(top, height))) : -1;
    out["mediantime"] = median_time_past(query, link);

    // Cumulative work to this block, big-endian per bitcoind chainwork.
    uint256_t work{};
    if (query.get_work(work, link))
        out["chainwork"] = encode_hash(from_uintx(work));

    if (header.previous_block_hash() != null_hash)
        out["previousblockhash"] = encode_hash(header.previous_block_hash());

    if (confirmed && height < top)
        out["nextblockhash"] = encode_hash(
            query.get_header_key(query.to_confirmed(add1(height))));
}

void protocol_bitcoind_rpc::inject_tx_context(boost::json::object& out,
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

boost::json::object protocol_bitcoind_rpc::header_to_bitcoind(
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

std::string protocol_bitcoind_rpc::chain_name(const node::query& query) NOEXCEPT
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

} // namespace server
} // namespace libbitcoin
