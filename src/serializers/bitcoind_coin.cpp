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
#include <bitcoin/server/serializers/bitcoind_coin.hpp>

#include <unordered_map>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/bitcoind_json.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

uint64_t total_subsidy(const system::settings& settings,
    size_t height) NOEXCEPT
{
    uint64_t total{};
    for (auto index = zero; index <= height; ++index)
        total += chain::block::subsidy(index, settings.subsidy_interval_blocks,
            settings.initial_subsidy(), settings.forks.bip42);

    return total;
}

// The block is read and populated, as bitcoind's index accumulates these.
code block_info(network::rpc::object_t& out, const node::query& query,
    const system::settings& settings, const database::header_link& link,
    size_t height) NOEXCEPT
{
    database::block_amounts amounts{};
    if (const auto ec = query.get_block_amounts(amounts, link))
        return ec;

    // Coins issued but not claimed by the coinbase are destroyed.
    const auto fees = floored_subtract(amounts.prevouts, amounts.outputs);
    const auto reward = chain::block::subsidy(height,
        settings.subsidy_interval_blocks, settings.initial_subsidy(),
        settings.forks.bip42) + fees;
    const auto unclaimed = floored_subtract(reward, amounts.coinbase);
    const auto unspendable = unclaimed + amounts.unspendable + amounts.bip30;

    network::rpc::object_t unspendables
    {
        { "genesis_block", to_btc(0_u64) },
        { "bip30", to_btc(amounts.bip30) },
        { "scripts", to_btc(amounts.unspendable) },
        { "unclaimed_rewards", to_btc(unclaimed) }
    };

    out = network::rpc::object_t
    {
        { "prevout_spent", to_btc(amounts.prevouts) },
        { "coinbase", to_btc(amounts.coinbase) },
        { "new_outputs_ex_coinbase", to_btc(amounts.outputs) },
        { "unspendable", to_btc(unspendable) },
        { "unspendables", std::move(unspendables) }
    };

    return error::success;
}

network::rpc::object_t scan_result(
    database::unspent_coins& coins, const node::query& query,
    const chain::scripts& scripts, size_t top, uint64_t txouts, bool bip30,
    uint8_t p2kh, uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    using namespace network::rpc;

    // The needle set, keyed by output script hash (the address index key).
    struct needle { std::string hex; std::string desc; };
    std::unordered_map<hash_digest, needle> needles{};
    for (const auto& script: scripts)
    {
        if (script.is_unspendable())
            continue;

        const auto data = script.to_data(false);
        const auto key = sha256_hash(data);
        auto hex = encode_base16(data);
        auto desc = infer_descriptor(script, p2kh, p2sh, witness);
        needles.emplace(key, needle{ std::move(hex), std::move(desc) });
    }

    // Report in canonical (txid, index) order.
    std::sort(coins.begin(), coins.end(),
        [](const auto& left, const auto& right) NOEXCEPT
        {
            const auto& one = left.out.point();
            const auto& two = right.out.point();
            if (one.hash() == two.hash())
                return one.index() < two.index();

            return one.hash() < two.hash();
        });

    uint64_t amount{};
    array_t unspents{};
    for (auto& coin: coins)
    {
        const auto found = needles.find(accumulator<sha256>::hash(
            coin.script));
        if (found == needles.cend())
            continue;

        // bitcoind retains duplicated coinbases at the overwriting heights.
        if (bip30 && coin.coinbase)
            coin.height = (coin.height == 91812) ? 91842 :
                (coin.height == 91722) ? 91880 : coin.height;

        const auto link = query.to_confirmed(coin.height);
        unspents.emplace_back(object_t
        {
            { "txid", encode_hash(coin.out.point().hash()) },
            { "vout", coin.out.point().index() },
            { "scriptPubKey", found->second.hex },
            { "desc", found->second.desc },
            { "amount", to_btc(coin.out.value()) },
            { "coinbase", coin.coinbase },
            { "height", to64(coin.height) },
            { "blockhash", encode_hash(query.get_header_key(link)) },
            { "confirmations", to64(add1(floored_subtract(top, coin.height))) }
        });

        amount += coin.out.value();
    }

    const auto best = query.to_confirmed(top);
    return object_t
    {
        { "success", true },
        { "txouts", txouts },
        { "height", to64(top) },
        { "bestblock", encode_hash(query.get_header_key(best)) },
        { "unspents", std::move(unspents) },
        { "total_amount", to_btc(amount) }
    };
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
