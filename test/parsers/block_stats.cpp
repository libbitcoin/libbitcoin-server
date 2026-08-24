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
#include "../test.hpp"

BOOST_AUTO_TEST_SUITE(block_stats_tests)

using namespace system;
using namespace system::chain;

constexpr uint64_t test_subsidy = 5'000'000'000;

static transaction make_coinbase(const script& out_script = {}) NOEXCEPT
{
    return { 1, inputs{ { point{}, script{}, 0 } },
        outputs{ { test_subsidy, out_script } }, 0 };
}

// A paying tx with a populated prevout of the given value.
static transaction make_paying(uint64_t in_value, uint64_t out_value) NOEXCEPT
{
    transaction tx{ 1, inputs{ { point{ one_hash, 0 }, script{}, 0 } },
        outputs{ { out_value, script{} } }, 0 };
    tx.inputs_ptr()->front()->prevout = to_shared<output>(in_value, script{});
    return tx;
}

static block make_block(transactions&& txs) NOEXCEPT
{
    return { header{ 1, null_hash, null_hash, 42, 0x1d00ffff, 0 },
        std::move(txs) };
}

BOOST_AUTO_TEST_CASE(block_stats__coinbase_only__no_fees)
{
    const auto block = make_block({ make_coinbase() });
    const auto stats = server::block_stats(block, 1, 40, test_subsidy);

    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("txs").value()), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("ins").value()), 0u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("outs").value()), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("totalfee").value()), 0u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("minfee").value()), 0u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("subsidy").value()), test_subsidy);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(stats.at("mediantime").value()), 40u);
    BOOST_REQUIRE_EQUAL(std::get<int64_t>(stats.at("utxo_increase").value()), 1);
    BOOST_REQUIRE_EQUAL(std::get<int64_t>(stats.at("utxo_increase_actual").value()), 1);
}

BOOST_AUTO_TEST_CASE(block_stats__two_paying__fee_statistics)
{
    const auto block = make_block({ make_coinbase(),
        make_paying(100'000, 90'000), make_paying(50'000, 48'000) });
    const auto stats = server::block_stats(block, 2, 40, test_subsidy);

    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("txs").value()), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("ins").value()), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("outs").value()), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("totalfee").value()), 12'000u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("minfee").value()), 2'000u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("maxfee").value()), 10'000u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("avgfee").value()), 6'000u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("medianfee").value()), 6'000u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("total_out").value()), 138'000u);
    BOOST_REQUIRE_EQUAL(std::get<int64_t>(stats.at("utxo_increase").value()), 1);

    // Both paying txs are identical in size/weight.
    const auto weight = block.transactions_ptr()->back()->weight();
    const auto rate = (4u * 10'000u) / weight;
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(stats.at("maxfeerate").value()), rate);

    // The top percentile is the maximum feerate.
    const auto& rates = std::get<network::rpc::array_t>(stats.at("feerate_percentiles").value());
    BOOST_REQUIRE_EQUAL(rates.size(), 5u);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(rates.back().value()), rate);
}

// Unspendable outputs are excluded from the actual utxo statistics.
BOOST_AUTO_TEST_CASE(block_stats__unspendable_output__excluded_from_actual)
{
    const script unspendable{ operations{ operation{ opcode::op_return } } };
    const auto block = make_block({ make_coinbase(unspendable) });
    const auto stats = server::block_stats(block, 1, 40, test_subsidy);

    BOOST_REQUIRE_EQUAL(std::get<int64_t>(stats.at("utxo_increase").value()), 1);
    BOOST_REQUIRE_EQUAL(std::get<int64_t>(stats.at("utxo_increase_actual").value()), 0);
}

BOOST_AUTO_TEST_SUITE_END()
