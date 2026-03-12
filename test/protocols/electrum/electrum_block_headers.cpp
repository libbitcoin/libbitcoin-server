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

// blockchain.block.headers

using namespace system;
static const code not_found{ server::error::not_found };
static const code target_overflow{ server::error::target_overflow };
static const code invalid_argument{ server::error::invalid_argument };

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__genesis_count1_no_checkpoint__expected)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":60,"method":"blockchain.block.headers","params":[0,1]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("max").as_int64(), 5);
    BOOST_CHECK_EQUAL(result.at("count").as_int64(), 1);
    BOOST_CHECK(result.at("headers").is_array());
    BOOST_CHECK_EQUAL(result.at("headers").as_array().size(), 1u);
    BOOST_CHECK_EQUAL(result.at("headers").as_array().at(0).as_string(), encode_base16(header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__block1to3_no_checkpoint__expected)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":61,"method":"blockchain.block.headers","params":[1,3]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("max").as_int64(), 5);
    BOOST_CHECK_EQUAL(result.at("count").as_int64(), 3);
    BOOST_CHECK(result.at("headers").is_array());

    const auto& headers = result.at("headers").as_array();
    BOOST_CHECK_EQUAL(headers.size(), 3u);
    BOOST_CHECK_EQUAL(headers.at(0).as_string(), encode_base16(header1_data));
    BOOST_CHECK_EQUAL(headers.at(1).as_string(), encode_base16(header2_data));
    BOOST_CHECK_EQUAL(headers.at(2).as_string(), encode_base16(header3_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__count_exceeds_max__capped)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":62,"method":"blockchain.block.headers","params":[0,10]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("max").as_int64(), 5);
    BOOST_CHECK_EQUAL(result.at("count").as_int64(), 5);
    BOOST_CHECK_EQUAL(result.at("headers").as_array().size(), 5u);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__count_zero__empty_headers)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":63,"method":"blockchain.block.headers","params":[5,0]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("count").as_int64(), 0);
    BOOST_CHECK(result.at("headers").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__proof_no_offset__expected)
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

    const auto response = get(R"({"id":64,"method":"blockchain.block.headers","params":[5,1,8]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("max").as_int64(), 5);
    BOOST_CHECK_EQUAL(result.at("count").as_int64(), 1);
    BOOST_CHECK_EQUAL(result.at("headers").as_array().size(), 1u);
    BOOST_CHECK_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_CHECK_EQUAL(branch.size(), expected_branch.size());
    BOOST_CHECK_EQUAL(branch.at(0).as_string(), expected_branch[0]);
    BOOST_CHECK_EQUAL(branch.at(1).as_string(), expected_branch[1]);
    BOOST_CHECK_EQUAL(branch.at(2).as_string(), expected_branch[2]);
    BOOST_CHECK_EQUAL(branch.at(3).as_string(), expected_branch[3]);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__proof_offset__expected)
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
        encode_hash(block6_hash),
        encode_hash(root45),
        encode_hash(root03),
        encode_hash(root88)
    };

    const auto response = get(R"({"id":64,"method":"blockchain.block.headers","params":[5,3,8]})" "\n");
    const auto& result = response.at("result").as_object();
    BOOST_CHECK_EQUAL(result.at("max").as_int64(), 5);
    BOOST_CHECK_EQUAL(result.at("count").as_int64(), 3);
    BOOST_CHECK_EQUAL(result.at("headers").as_array().size(), 3u);
    BOOST_CHECK_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_CHECK_EQUAL(branch.size(), expected_branch.size());
    BOOST_CHECK_EQUAL(branch.at(0).as_string(), expected_branch[0]);
    BOOST_CHECK_EQUAL(branch.at(1).as_string(), expected_branch[1]);
    BOOST_CHECK_EQUAL(branch.at(2).as_string(), expected_branch[2]);
    BOOST_CHECK_EQUAL(branch.at(3).as_string(), expected_branch[3]);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__start_above_top__not_found)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":65,"method":"blockchain.block.headers","params":[10,1]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__target_exceeds_waypoint__target_overflow)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":66,"method":"blockchain.block.headers","params":[2,3,1]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), target_overflow.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__waypoint_above_top__not_found)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":67,"method":"blockchain.block.headers","params":[0,1,10]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__negative_start__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":68,"method":"blockchain.block.headers","params":[-1,1]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__fractional_count__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":69,"method":"blockchain.block.headers","params":[0,1.5]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__start_plus_count_huge__not_found)
{
    BOOST_CHECK(handshake());

    // argument_overflow is not actually reachable via json due to its integer limits.
    const auto response = get(R"({"id":70,"method":"blockchain.block.headers","params":[9007199254740991,2]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_SUITE_END()
