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

BC_AUTO_TEST_SUITE(coinbase_tests)

using namespace system;
using namespace system::chain;

// Weight of a serialized coinbase of the given script and payout sizes.
static size_t serialized_weight(size_t script, size_t payout) NOEXCEPT
{
    constexpr auto commitment = two + sizeof(witness_head) + hash_size;
    const data_stack reserved{ data_chunk(hash_size, 0x00) };
    inputs ins
    {
        input
        {
            point{},
            chain::script{ data_chunk(script, 0x00), false },
            witness{ reserved },
            max_uint32
        }
    };

    outputs outs
    {
        output{ zero, chain::script{ data_chunk(payout, 0x00), false } },
        output{ zero, chain::script{ data_chunk(commitment, 0x00), false } }
    };

    return transaction{ 1, std::move(ins), std::move(outs), 0 }.weight();
}

// A p2wpkh payout script of the given hash.
static script payout_script(uint8_t fill) NOEXCEPT
{
    const short_hash hash{ fill };
    return script{ script::to_pay_witness_key_hash_pattern(hash) };
}

// The witness commitment carried by a serialized coinbase, null if none.
static hash_digest commitment_of(const transaction& tx) NOEXCEPT
{
    hash_cref commitment{ null_hash };
    return tx.get_witness_commitment(commitment) ? commitment.get() :
        null_hash;
}

// The non-witness coinbase of prefix, extranonce and suffix.
static data_chunk unwitnessed(const data_chunk& prefix,
    const data_chunk& extranonce, const data_chunk& suffix) NOEXCEPT
{
    return splice(splice(prefix, extranonce), suffix);
}

BC_AUTO_TEST_CASE(coinbase__coinbase_script_size__untagged__height_and_extranonce)
{
    BC_REQUIRE_EQUAL(coinbase_script_size(0, 8), 14u);
}

BC_AUTO_TEST_CASE(coinbase__coinbase_script_size__tagged__height_tag_and_extranonce)
{
    BC_REQUIRE_EQUAL(coinbase_script_size(3, 8), 18u);
}

BC_AUTO_TEST_CASE(coinbase__coinbase_weight__largest_address__expected)
{
    BC_REQUIRE_EQUAL(coinbase_weight(14, max_address_script), 656u);
}

BC_AUTO_TEST_CASE(coinbase__coinbase_weight__largest_address__serialized_weight)
{
    BC_REQUIRE_EQUAL(coinbase_weight(14, max_address_script), serialized_weight(14, max_address_script));
}

BC_AUTO_TEST_CASE(coinbase__coinbase_weight__p2wpkh_tagged__serialized_weight)
{
    BC_REQUIRE_EQUAL(coinbase_weight(18, 22), serialized_weight(18, 22));
}

BC_AUTO_TEST_CASE(coinbase__coinbase_prefix__height_one_untagged__expected)
{
    const auto prefix = coinbase_prefix(1, "", 8);
    BC_REQUIRE_EQUAL(encode_base16(prefix), "0200000001" + encode_base16(null_hash) + "ffffffff0a5108");
}

BC_AUTO_TEST_CASE(coinbase__coinbase_prefix__tagged__tag_pushed_before_extranonce)
{
    const auto prefix = coinbase_prefix(1, "/x/", 8);
    BC_REQUIRE_EQUAL(encode_base16(prefix), "0200000001" + encode_base16(null_hash) + "ffffffff0e51032f782f08");
}

BC_AUTO_TEST_CASE(coinbase__coinbase_suffix__unwitnessed__valid_coinbase_of_height)
{
    const data_chunk extranonce(8, 0x42);
    const auto prefix = coinbase_prefix(500'000, "/x/", 8);
    const auto suffix = coinbase_suffix(500'000, 50, payout_script(0x01), hash_digest{ 0x07 });
    const transaction tx{ unwitnessed(prefix, extranonce, suffix), false };
    BC_REQUIRE(tx.is_valid());
    BC_REQUIRE(tx.is_coinbase());
    BC_REQUIRE(script::is_coinbase_pattern(tx.inputs_ptr()->front()->script().ops(), 500'000));
    BC_REQUIRE_EQUAL(tx.outputs_ptr()->front()->value(), 50u);
    BC_REQUIRE(tx.outputs_ptr()->front()->script() == payout_script(0x01));
    BC_REQUIRE_EQUAL(commitment_of(tx), hash_digest{ 0x07 });
    BC_REQUIRE_EQUAL(tx.locktime(), 499'999u);
    BC_REQUIRE(!tx.inputs_ptr()->front()->is_final());
}

BC_AUTO_TEST_CASE(coinbase__coinbase_suffix__uncommitted__valid_coinbase_of_one_output)
{
    const data_chunk extranonce(8, 0x42);
    const auto prefix = coinbase_prefix(500'000, "", 8);
    const auto suffix = coinbase_suffix(500'000, 50, payout_script(0x01));
    const transaction tx{ coinbase_unwitnessed(prefix, extranonce, suffix), false };
    BC_REQUIRE(tx.is_valid());
    BC_REQUIRE(tx.is_coinbase());
    BC_REQUIRE_EQUAL(tx.outputs_ptr()->size(), 1u);
    BC_REQUIRE_EQUAL(tx.outputs_ptr()->front()->value(), 50u);
    BC_REQUIRE(tx.outputs_ptr()->front()->script() == payout_script(0x01));
    BC_REQUIRE_EQUAL(tx.locktime(), 499'999u);
    BC_REQUIRE(!tx.inputs_ptr()->front()->is_final());
}

BC_AUTO_TEST_CASE(coinbase__coinbase_witnessed__valid__same_txid_null_reserved)
{
    const data_chunk extranonce(8, 0x42);
    const auto prefix = coinbase_prefix(500'000, "", 8);
    const auto suffix = coinbase_suffix(500'000, 50, payout_script(0x01), hash_digest{ 0x07 });
    const transaction tx{ coinbase_witnessed(prefix, extranonce, suffix), true };
    BC_REQUIRE(tx.is_valid());
    BC_REQUIRE(tx.is_segregated());
    BC_REQUIRE_EQUAL(tx.hash(false), bitcoin_hash(unwitnessed(prefix, extranonce, suffix)));
    BC_REQUIRE_EQUAL(commitment_of(tx), hash_digest{ 0x07 });
}

BC_AUTO_TEST_CASE(coinbase__coinbase_witnessed__largest_height__within_reserved_weight)
{
    const data_chunk extranonce(8, 0x42);
    const auto prefix = coinbase_prefix(max_int32, "/x/", 8);
    const auto suffix = coinbase_suffix(max_int32, 50, payout_script(0x01), null_hash);
    const transaction tx{ coinbase_witnessed(prefix, extranonce, suffix), true };
    BC_REQUIRE_EQUAL(tx.weight(), coinbase_weight(coinbase_script_size(3, 8), 22));
}

BC_AUTO_TEST_CASE(coinbase__coinbase_root__empty_branch__txid)
{
    BC_REQUIRE_EQUAL(coinbase_root(hash_digest{ 0x01 }, {}), hash_digest{ 0x01 });
}

BC_AUTO_TEST_CASE(coinbase__coinbase_root__two_branches__folded_right)
{
    const hashes branch{ hash_digest{ 0x02 }, hash_digest{ 0x03 } };
    const auto expected = sha256::double_hash(sha256::double_hash(hash_digest{ 0x01 }, hash_digest{ 0x02 }), hash_digest{ 0x03 });
    BC_REQUIRE_EQUAL(coinbase_root(hash_digest{ 0x01 }, branch), expected);
}

BC_AUTO_TEST_SUITE_END()
