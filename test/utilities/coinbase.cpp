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

BC_AUTO_TEST_SUITE_END()
