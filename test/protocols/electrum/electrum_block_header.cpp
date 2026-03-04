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
#include "../../test.hpp"
#include "electrum.hpp"

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

// blockchain.block.header

using namespace system;
static const code not_found{ server::error::not_found };
static const code target_overflow{ server::error::target_overflow };
static const code invalid_argument{ server::error::invalid_argument };

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__genesis_no_checkpoint__expected_no_proof)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":43,"method":"blockchain.block.header","params":[0]})" "\n");
    BOOST_CHECK_EQUAL(response.at("result").as_object().at("header").as_string(), encode_base16(header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__block1_no_checkpoint__expected_no_proof)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":44,"method":"blockchain.block.header","params":[1]})" "\n");
    BOOST_CHECK_EQUAL(response.at("result").as_object().at("header").as_string(), encode_base16(header1_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__genesis_zero_checkpoint__expected_no_proof)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":45,"method":"blockchain.block.header","params":[0,0]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("header").as_string(), encode_base16(header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__proof_self_block1__expected)
{
    BOOST_CHECK(handshake());
    const auto expected_header = encode_base16(header1_data);
    const auto expected_root = encode_hash(merkle_root(
    {
        block0_hash,
        block1_hash
    }));

    const auto response = get(R"({"id":46,"method":"blockchain.block.header","params":[1,1]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("header").as_string(), expected_header);
    BOOST_CHECK_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_CHECK_EQUAL(branch.size(), 1u);
    BOOST_CHECK_EQUAL(branch.at(0).as_string(), encode_hash(block0_hash));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__proof_example__expected)
{
    BOOST_CHECK(handshake());

    const auto expected_root = encode_hash(merkle_root(
    {
        block0_hash,
        block1_hash,
        block2_hash,
        block3_hash,
        block4_hash,
        block5_hash,
        block6_hash,
        block7_hash,
        block8_hash
    }));

    const string_list expected_branch
    {
        encode_hash(block4_hash),
        encode_hash(root67),
        encode_hash(root03),
        encode_hash(root88)
    };

    const auto response = get(R"({"id":50,"method":"blockchain.block.header","params":[5,8]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("header").as_string(), encode_base16(header5_data));
    BOOST_CHECK_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_CHECK_EQUAL(branch.size(), expected_branch.size());
    BOOST_CHECK_EQUAL(branch.at(0).as_string(), expected_branch[0]);
    BOOST_CHECK_EQUAL(branch.at(1).as_string(), expected_branch[1]);
    BOOST_CHECK_EQUAL(branch.at(2).as_string(), expected_branch[2]);
    BOOST_CHECK_EQUAL(branch.at(3).as_string(), expected_branch[3]);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__cp_below_height__target_overflow)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":51,"method":"blockchain.block.header","params":[2,1]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), target_overflow.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__above_top__not_found)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":52,"method":"blockchain.block.header","params":[10]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__cp_above_top__not_found)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":53,"method":"blockchain.block.header","params":[1,10]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__negative_height__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":54,"method":"blockchain.block.header","params":[-1]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__fractional_height__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":55,"method":"blockchain.block.header","params":[1.5]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__over_top_height__not_found)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":56,"method":"blockchain.block.header","params":[4294967296]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_SUITE_END()
