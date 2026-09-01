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

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void to_coin_data(data_chunk& out,
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
bool block_info(network::rpc::object_t& out, const node::query& query,
    const system::settings& settings, const database::header_link& link,
    size_t height) NOEXCEPT
{
    database::block_amounts amounts{};
    if (query.get_block_amounts(amounts, link))
        return false;

    // Coins issued but not claimed by the coinbase are destroyed.
    const auto fees = floored_subtract(amounts.prevouts, amounts.outputs);
    const auto reward = chain::block::subsidy(height,
        settings.subsidy_interval_blocks, settings.initial_subsidy(),
        settings.forks.bip42) + fees;
    const auto unclaimed = floored_subtract(reward, amounts.coinbase);
    const auto unspendable = unclaimed + amounts.unspendable + amounts.bip30;
    const auto bitcoin = to_floating(chain::satoshi_per_bitcoin);

    out = network::rpc::object_t
    {
        { "prevout_spent", amounts.prevouts / bitcoin },
        { "coinbase", amounts.coinbase / bitcoin },
        { "new_outputs_ex_coinbase", amounts.outputs / bitcoin },
        { "unspendable", unspendable / bitcoin },
        { "unspendables", network::rpc::object_t
        {
            { "genesis_block", zero },
            { "bip30", amounts.bip30 / bitcoin },
            { "scripts", amounts.unspendable / bitcoin },
            { "unclaimed_rewards", unclaimed / bitcoin }
        }}
    };

    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
