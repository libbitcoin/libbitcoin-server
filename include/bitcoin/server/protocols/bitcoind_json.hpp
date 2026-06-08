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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_BITCOIND_JSON_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_BITCOIND_JSON_HPP

#include <algorithm>
#include <string>
#include <vector>
#include <bitcoin/server/define.hpp>
#include <bitcoin/system/chain/json/json.hpp>

namespace libbitcoin {
namespace server {

/// Shared json helpers for the bitcoind rpc and rest protocols. The bitcoind
/// block/header serializers in libbitcoin-system intentionally omit chain
/// context (height, confirmations, etc.); these add it at the protocol layer.

/// BIP113 median of up to 11 block timestamps ending at the given height.
inline uint32_t median_time_past(const auto& query, size_t height) NOEXCEPT
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

/// Add the chain-context fields the bitcoind block/header serializers omit.
inline void inject_block_context(boost::json::object& out, const auto& query,
    const database::header_link& link,
    const system::chain::header& header) NOEXCEPT
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

    if (header.previous_block_hash() != system::null_hash)
        out["previousblockhash"] =
            system::encode_hash(header.previous_block_hash());

    if (confirmed && height < top)
        out["nextblockhash"] = system::encode_hash(
            query.get_header_key(query.to_confirmed(height + 1u)));
}

/// Add the chain-context fields the bitcoind tx serializer omits (verbose tx).
/// For a confirmed tx: in_active_chain/blockhash/confirmations/blocktime/time.
/// For an archived-but-unconfirmed tx: confirmations = 0 (no block fields).
inline void inject_tx_context(boost::json::object& out, const auto& query,
    const database::tx_link& link) NOEXCEPT
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
    out["blockhash"] = system::encode_hash(query.get_header_key(block));
    out["confirmations"] = static_cast<uint64_t>(top - height + 1u);
    if (header)
    {
        out["blocktime"] = header->timestamp();
        out["time"] = header->timestamp();
    }
}

/// Build a bitcoind-format block header object (no system serializer exists).
inline boost::json::object header_to_bitcoind(
    const system::chain::header& header) NOEXCEPT
{
    using namespace system;
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

/// Map the genesis block hash to Bitcoin Core's "chain" identifier.
inline std::string chain_name(const auto& query) NOEXCEPT
{
    const auto genesis = system::encode_hash(
        query.get_header_key(query.to_confirmed(0)));

    if (genesis == "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f")
        return "main";
    if (genesis == "000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943")
        return "test";
    if (genesis == "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6")
        return "signet";
    if (genesis == "0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")
        return "regtest";

    return "unknown";
}

} // namespace server
} // namespace libbitcoin

#endif
