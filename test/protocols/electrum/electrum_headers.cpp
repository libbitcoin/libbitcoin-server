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

using namespace system;
static const code not_found{ server::error::not_found };
static const code wrong_version{ server::error::wrong_version };
static const code target_overflow{ server::error::target_overflow };
static const code invalid_argument{ server::error::invalid_argument };

// blockchain.numblocks.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_number_of_blocks_subscribe__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":1001,"method":"blockchain.numblocks.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_number_of_blocks_subscribe__9_block_store__returns_9)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":1004,"method":"blockchain.numblocks.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

// blockchain.block.get_chunk

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_chunk__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_chunk","params":[0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_chunk__invalid_index__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_chunk","params":[42.42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_chunk__above_top__empty)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_chunk","params":[1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE(response.at("result").as_string().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_chunk__zero__first_ten_headers_in_order)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));
    using namespace test;
    const auto expected = encode_base16(header0_data) +
        encode_base16(header1_data) + encode_base16(header2_data) + encode_base16(header3_data) +
        encode_base16(header4_data) + encode_base16(header5_data) + encode_base16(header6_data) +
        encode_base16(header7_data) + encode_base16(header8_data) + encode_base16(header9_data);

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_chunk","params":[0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    const auto& result = response.at("result").as_string();
    BOOST_REQUIRE_EQUAL(result, expected);
}

// blockchain.block.get_header

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_header__obsoleted_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_header","params":[0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_header__invalid_height__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_header","params":[42.42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_header__above_top__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_header","params":[42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_get_header__five__expected_header)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":43,"method":"blockchain.block.get_header","params":[5]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    const auto& result = response.at("result").as_string();
    BOOST_REQUIRE_EQUAL(result, encode_base16(test::header5_data));
}

// blockchain.block.header

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":43,"method":"blockchain.block.header","params":[0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__genesis_no_checkpoint__expected_no_proof)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_3));

    const auto response = get(R"({"id":43,"method":"blockchain.block.header","params":[0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    // "hex" prior to v1.6
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("hex").is_string());
    BOOST_REQUIRE_EQUAL(result.at("hex").as_string(), encode_base16(test::header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__block1_no_checkpoint__expected_no_proof)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":44,"method":"blockchain.block.header","params":[1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").as_object().at("header").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_object().at("header").as_string(), encode_base16(test::header1_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__genesis_zero_checkpoint__expected_no_proof)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":45,"method":"blockchain.block.header","params":[0,0]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").as_object().at("header").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_object().at("header").as_string(), encode_base16(test::header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__proof_self_block1__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));
    const auto expected_header = encode_base16(test::header1_data);
    const auto expected_root = encode_hash(merkle_root(
    {
        test::block0_hash,
        test::block1_hash
    }));

    const auto response = get(R"({"id":46,"method":"blockchain.block.header","params":[1,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("header").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("root").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("branch").is_array());
    BOOST_REQUIRE_EQUAL(result.at("header").as_string(), expected_header);
    BOOST_REQUIRE_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_REQUIRE(branch.at(0).is_string());
    BOOST_REQUIRE_EQUAL(branch.size(), 1u);
    BOOST_REQUIRE_EQUAL(branch.at(0).as_string(), encode_hash(test::block0_hash));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__proof_example__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));
    
    using namespace test;
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
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("header").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("root").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("branch").is_array());
    BOOST_REQUIRE_EQUAL(result.at("header").as_string(), encode_base16(header5_data));
    BOOST_REQUIRE_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_REQUIRE(branch.at(0).is_string());
    BOOST_REQUIRE_EQUAL(branch.size(), expected_branch.size());
    BOOST_REQUIRE_EQUAL(branch.at(0).as_string(), expected_branch[0]);
    BOOST_REQUIRE_EQUAL(branch.at(1).as_string(), expected_branch[1]);
    BOOST_REQUIRE_EQUAL(branch.at(2).as_string(), expected_branch[2]);
    BOOST_REQUIRE_EQUAL(branch.at(3).as_string(), expected_branch[3]);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__checkpoint_below_height__target_overflow)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":51,"method":"blockchain.block.header","params":[2,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), target_overflow.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__above_top__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":52,"method":"blockchain.block.header","params":[10]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__checkpoint_above_top__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":53,"method":"blockchain.block.header","params":[1,10]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__negative_height__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":54,"method":"blockchain.block.header","params":[-1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__fractional_height__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":55,"method":"blockchain.block.header","params":[1.5]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__over_top_height__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":56,"method":"blockchain.block.header","params":[4294967296]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

// blockchain.block.headers

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":60,"method":"blockchain.block.headers","params":[0,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__genesis_count1_no_checkpoint_v1_2__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":60,"method":"blockchain.block.headers","params":[0,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 1);

    // "hex" prior to 1.6
    REQUIRE_NO_THROW_TRUE(result.at("hex").is_string());
    BOOST_REQUIRE_EQUAL(result.at("hex").as_string(), encode_base16(test::header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__genesis_count1_no_checkpoint_v1_6__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":60,"method":"blockchain.block.headers","params":[0,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 1);
    REQUIRE_NO_THROW_TRUE(result.at("headers").is_array());
    BOOST_REQUIRE(result.at("headers").as_array().at(0).is_string());
    BOOST_REQUIRE_EQUAL(result.at("headers").as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(result.at("headers").as_array().at(0).as_string(), encode_base16(test::header0_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__block1to3_no_checkpoint_v1_6__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":61,"method":"blockchain.block.headers","params":[1,3]})" "\n");
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 3);
    REQUIRE_NO_THROW_TRUE(result.at("headers").is_array());

    const auto& headers = result.at("headers").as_array();
    BOOST_REQUIRE(result.at("headers").as_array().at(0).is_string());
    BOOST_REQUIRE_EQUAL(headers.size(), 3u);
    BOOST_REQUIRE_EQUAL(headers.at(0).as_string(), encode_base16(test::header1_data));
    BOOST_REQUIRE_EQUAL(headers.at(1).as_string(), encode_base16(test::header2_data));
    BOOST_REQUIRE_EQUAL(headers.at(2).as_string(), encode_base16(test::header3_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__block1to3_no_checkpoint_v1_4__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":61,"method":"blockchain.block.headers","params":[1,3]})" "\n");
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("hex").is_string());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 3);

    // "hex" prior to v1.6
    using namespace test;
    const auto expected = encode_base16(header1_data) + encode_base16(header2_data) + encode_base16(header3_data);
    BOOST_REQUIRE_EQUAL(result.at("hex").as_string(), expected);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__count_exceeds_max__capped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":62,"method":"blockchain.block.headers","params":[0,10]})" "\n");
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("headers").is_array());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("headers").as_array().size(), 5u);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__count_zero__empty_headers)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":63,"method":"blockchain.block.headers","params":[5,0]})" "\n");
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("headers").is_array());
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 0);
    BOOST_REQUIRE(result.at("headers").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__proof_no_offset__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    using namespace test;
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
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("headers").is_array());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("headers").as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_REQUIRE(branch.at(0).is_string());
    BOOST_REQUIRE_EQUAL(branch.size(), expected_branch.size());
    BOOST_REQUIRE_EQUAL(branch.at(0).as_string(), expected_branch[0]);
    BOOST_REQUIRE_EQUAL(branch.at(1).as_string(), expected_branch[1]);
    BOOST_REQUIRE_EQUAL(branch.at(2).as_string(), expected_branch[2]);
    BOOST_REQUIRE_EQUAL(branch.at(3).as_string(), expected_branch[3]);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__proof_offset__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    using namespace test;
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
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("max").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("count").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("headers").is_array());
    REQUIRE_NO_THROW_TRUE(result.at("root").is_string());
    BOOST_REQUIRE_EQUAL(result.at("max").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("count").as_int64(), 3);
    BOOST_REQUIRE_EQUAL(result.at("headers").as_array().size(), 3u);
    BOOST_REQUIRE_EQUAL(result.at("root").as_string(), expected_root);

    const auto& branch = result.at("branch").as_array();
    BOOST_REQUIRE(branch.at(0).is_string());
    BOOST_REQUIRE_EQUAL(branch.size(), expected_branch.size());
    BOOST_REQUIRE_EQUAL(branch.at(0).as_string(), expected_branch[0]);
    BOOST_REQUIRE_EQUAL(branch.at(1).as_string(), expected_branch[1]);
    BOOST_REQUIRE_EQUAL(branch.at(2).as_string(), expected_branch[2]);
    BOOST_REQUIRE_EQUAL(branch.at(3).as_string(), expected_branch[3]);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__start_above_top__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":65,"method":"blockchain.block.headers","params":[10,1]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__target_exceeds_waypoint__target_overflow)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":66,"method":"blockchain.block.headers","params":[2,3,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), target_overflow.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__waypoint_above_top__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":67,"method":"blockchain.block.headers","params":[0,1,10]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__negative_start__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":68,"method":"blockchain.block.headers","params":[-1,1]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__fractional_count__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":69,"method":"blockchain.block.headers","params":[0,1.5]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_headers__start_plus_count_huge__not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    // argument_overflow is not actually reachable via json due to its integer limits.
    const auto response = get(R"({"id":70,"method":"blockchain.block.headers","params":[9007199254740991,2]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_found.value());
}

// TODO: add optional bool parameter "raw".
// blockchain.headers.subscribe

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__default__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":80,"method":"blockchain.headers.subscribe","params":[]})" "\n");
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("hex").is_string());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("hex").as_string(), system::encode_base16(test::header9_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__jsonrpc_2__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"jsonrpc":"2.0","id":81,"method":"blockchain.headers.subscribe"})" "\n");
    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("height").is_int64());
    REQUIRE_NO_THROW_TRUE(result.at("hex").is_string());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("hex").as_string(), system::encode_base16(test::header9_data));
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__id_preserved__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":123,"method":"blockchain.headers.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 123);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_headers_subscribe__empty_params__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":82,"method":"blockchain.headers.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("hex").as_string(), system::encode_base16(test::header9_data));
}

BOOST_AUTO_TEST_SUITE_END()
