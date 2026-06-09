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
#include <vector>
#include <bitcoin/server/define.hpp>
#include <bitcoin/system/chain/json/json.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

uint32_t protocol_bitcoind_rpc::median_time_past(const node::query& query,
    size_t height) NOEXCEPT
{
    constexpr size_t window = 11;
    const auto count = std::min(window, height + 1u);
    std::vector<uint32_t> times{};
    times.reserve(count);
    for (size_t index = 0; index < count; ++index)
    {
        const auto header = query.get_header(query.to_confirmed(height - index));
        if (header)
            times.push_back(header->timestamp());
    }

    if (times.empty())
        return 0;

    std::sort(times.begin(), times.end());
    return times.at(times.size() / 2u);
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
    out["height"] = static_cast<uint64_t>(height);
    out["confirmations"] = confirmed ?
        static_cast<int64_t>(top - height + 1u) : int64_t{ -1 };
    out["mediantime"] = median_time_past(query, height);

    if (header.previous_block_hash() != null_hash)
        out["previousblockhash"] = encode_hash(header.previous_block_hash());

    if (confirmed && height < top)
        out["nextblockhash"] = encode_hash(
            query.get_header_key(query.to_confirmed(height + 1u)));
}

void protocol_bitcoind_rpc::inject_tx_context(boost::json::object& out,
    const node::query& query, const database::tx_link& link) NOEXCEPT
{
    size_t height{};
    if (!query.is_confirmed_tx(link) || !query.get_tx_height(height, link))
    {
        out["confirmations"] = 0;
        return;
    }

    const auto block = query.to_confirmed(height);
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(block);
    out["in_active_chain"] = true;
    out["blockhash"] = encode_hash(query.get_header_key(block));
    out["confirmations"] = static_cast<uint64_t>(top - height + 1u);
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
        { "difficulty", header.difficulty() }
    };
}

std::string protocol_bitcoind_rpc::chain_name(const node::query& query) NOEXCEPT
{
    const auto genesis = query.get_header_key(query.to_confirmed(0));

    using selection = chain::selection;
    const std::pair<selection, std::string> networks[]
    {
        { selection::mainnet, "main" },
        { selection::testnet, "test" },
        { selection::regtest, "regtest" }
    };

    for (const auto& [network, name]: networks)
        if (system::settings{ network }.genesis_block.hash() == genesis)
            return name;

    // Signet is not yet modeled in system::settings (stubbed by genesis hash).
    if (encode_hash(genesis) ==
        "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6")
        return "signet";

    return "unknown";
}

} // namespace server
} // namespace libbitcoin
