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
#include "../../test.hpp"
#include "electrum_setup_fixture.hpp"

BC_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

using namespace system;
static const code wrong_version{ server::error::electrum::bad_request };
static const code not_implemented{ server::error::electrum::method_not_found };
static const code invalid_argument{ server::error::electrum::bad_request };

// unknown method

BC_AUTO_TEST_CASE(electrum__unknown_method__v1_0__method_not_found)
{
    BC_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":399,"method":"server.bogus","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 399);
}

// server.add_peer

BC_AUTO_TEST_CASE(electrum__server_add_peer__insufficient_version__wrong_version)
{
    BC_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":400,"method":"server.add_peer","params":[{}]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BC_AUTO_TEST_CASE(electrum__server_add_peer__v1_1__not_implemented)
{
    BC_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":401,"method":"server.add_peer","params":[{}]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

// server.banner

BC_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_no_params__invalid_request)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":42,"method":"server.banner"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_request);
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_named_params__invalid_request)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":42,"method":"server.banner","params":{}})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_request);
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_empty_params__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":42,"method":"server.banner","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BC_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

BC_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_1__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"1.0","id":42,"method":"server.banner","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BC_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

BC_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_2__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"2.0","id":42,"method":"server.banner"})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BC_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

// server.donation_address

BC_AUTO_TEST_CASE(electrum__server_donation_address__jsonrpc_1__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"1.0","id":43,"method":"server.donation_address","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BC_REQUIRE_EQUAL(response.at("result").as_string(), "donation_address");
}

BC_AUTO_TEST_CASE(electrum__server_donation_address__jsonrpc_2__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"2.0","id":43,"method":"server.donation_address"})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BC_REQUIRE_EQUAL(response.at("result").as_string(), "donation_address");
}

// server.features

BC_AUTO_TEST_CASE(electrum__server_features__no_params__invalid_request)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":301,"method":"server.features"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_request);
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__server_features__extra_param__invalid_params)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":302,"method":"server.features","params":["extra"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_params);
}

BC_AUTO_TEST_CASE(electrum__server_features__default_hosts__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("genesis_hash").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("hash_function").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("server_version").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_min").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_max").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("pruning").is_null());
    REQUIRE_NO_THROW_TRUE(result.at("hosts").is_object());

    const auto min = config_.server.electrum.protocol_minimum.to_string();
    const auto max = config_.server.electrum.protocol_maximum.to_string();
    const auto server_name = config_.server.electrum.server_name;
    const auto genesis_hash = encode_hash(test::genesis.hash());
    BC_REQUIRE_EQUAL(result.at("genesis_hash").as_string(), genesis_hash);
    BC_REQUIRE_EQUAL(result.at("hash_function").as_string(), "sha256");
    BC_REQUIRE_EQUAL(result.at("server_version").as_string(), server_name);
    BC_REQUIRE_EQUAL(result.at("protocol_min").as_string(), min);
    BC_REQUIRE_EQUAL(result.at("protocol_max").as_string(), max);

    const auto& hosts = result.at("hosts").as_object();
    BC_REQUIRE_EQUAL(hosts.size(), 1u);
    REQUIRE_NO_THROW_TRUE(hosts.at("").is_object());

    const auto& host = hosts.at("").as_object();
    REQUIRE_NO_THROW_TRUE(host.at("tcp_port").is_null());
    REQUIRE_NO_THROW_TRUE(host.at("ssl_port").is_null());
}

BC_AUTO_TEST_CASE(electrum__server_features__v1_7__hash_function_removed)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BC_REQUIRE(!result.contains("hash_function"));
    REQUIRE_NO_THROW_TRUE(result.at("genesis_hash").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("server_version").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_min").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_max").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("pruning").is_null());
    REQUIRE_NO_THROW_TRUE(result.at("hosts").is_object());
}

BC_AUTO_TEST_CASE(electrum__server_features__method_flavours_v1_7__supports_verbose_true_false)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("method_flavours").is_object());

    const auto& flavours = result.at("method_flavours").as_object();
    REQUIRE_NO_THROW_TRUE(flavours.at("blockchain.transaction.broadcast_package").is_object());

    const auto& package = flavours.at("blockchain.transaction.broadcast_package").as_object();
    REQUIRE_NO_THROW_TRUE(package.at("supports_verbose_true").is_bool());
    BC_REQUIRE(!package.at("supports_verbose_true").as_bool());
}

BC_AUTO_TEST_CASE(electrum__server_features__hosts__expected)
{
    config_.server.electrum.self_binds.emplace_back("tcp.com:80");
    config_.server.electrum.self_safes.emplace_back("ssl.com:443");
    BC_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("hosts").is_object());

    const auto& hosts = result.at("hosts").as_object();
    REQUIRE_NO_THROW_TRUE(hosts.at("tcp.com").is_object());
    REQUIRE_NO_THROW_TRUE(hosts.at("ssl.com").is_object());

    const auto& tcp = hosts.at("tcp.com").as_object();
    const auto& ssl = hosts.at("ssl.com").as_object();
    BC_REQUIRE_EQUAL(tcp.at("tcp_port").as_int64(), 80);
    BC_REQUIRE_EQUAL(ssl.at("ssl_port").as_int64(), 443);
}

// server.peers.subscribe

BC_AUTO_TEST_CASE(electrum__server_peers_subscribe__extra_param__invalid_params)
{
    BC_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":502,"method":"server.peers.subscribe","params":["extra"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_params);
}

BC_AUTO_TEST_CASE(electrum__server_peers_subscribe__empty_params__empty_array)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":503,"method":"server.peers.subscribe","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BC_REQUIRE(response.at("result").as_array().empty());
}

BC_AUTO_TEST_CASE(electrum__server_peers_subscribe__configured_peers__expected)
{
    config_.server.electrum.more_binds.emplace_back("another.trusted.server:50001");
    config_.server.electrum.more_safes.emplace_back("electrum.example.com");
    config_.server.electrum.more_safes.emplace_back("fulcrum.trustednode.org:50002");
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":504,"method":"server.peers.subscribe","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& peers = response.at("result").as_array();
    BC_REQUIRE_EQUAL(peers.size(), 3u);

    // Peer 0: another.trusted.server:50001
    const auto& peer0 = peers[0].as_array();
    BC_REQUIRE_EQUAL(peer0.size(), 3u);
    BC_REQUIRE_EQUAL(peer0[0].as_string(), "another.trusted.server");
    BC_REQUIRE_EQUAL(peer0[1].as_string(), "another.trusted.server");

    const auto& features0 = peer0[2].as_array();
    BC_REQUIRE_EQUAL(features0.size(), 1u);
    BC_REQUIRE_EQUAL(features0[0].as_string(), "t50001");

    // Peer 1: electrum.example.com (default SSL)
    const auto& peer1 = peers[1].as_array();
    BC_REQUIRE_EQUAL(peer1.size(), 3u);
    BC_REQUIRE_EQUAL(peer1[0].as_string(), "electrum.example.com");
    BC_REQUIRE_EQUAL(peer1[1].as_string(), "electrum.example.com");

    const auto& features1 = peer1[2].as_array();
    BC_REQUIRE_EQUAL(features1.size(), 1u);
    BC_REQUIRE_EQUAL(features1[0].as_string(), "s");

    // Peer 2: fulcrum.trustednode.org:50002
    const auto& peer2 = peers[2].as_array();
    BC_REQUIRE_EQUAL(peer2.size(), 3u);
    BC_REQUIRE_EQUAL(peer2[0].as_string(), "fulcrum.trustednode.org");
    BC_REQUIRE_EQUAL(peer2[1].as_string(), "fulcrum.trustednode.org");

    const auto& features2 = peer2[2].as_array();
    BC_REQUIRE_EQUAL(features2.size(), 1u);
    BC_REQUIRE_EQUAL(features2[0].as_string(), "s50002");
}

// server.ping

BC_AUTO_TEST_CASE(electrum__server_ping__v1_1__wrong_version)
{
    BC_REQUIRE(handshake(electrum::version::v1_1));

    const auto result = get_error(R"({"id":199,"method":"server.ping","params":[]})" "\n");
    BC_REQUIRE_EQUAL(result, wrong_version.value());
}

BC_AUTO_TEST_CASE(electrum__server_ping__jsonrpc_unspecified_no_params__invalid_request)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":201,"method":"server.ping"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_request);
    REQUIRE_NO_THROW_TRUE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(electrum__server_ping__extra_param__invalid_params)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":215,"method":"server.ping","params":[8,"12345678", 42]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BC_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), network::rpc::reserved::invalid_params);
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_2_defaults__null)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":200,"method":"server.ping","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_2_arguments__null)
{
    BC_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":202,"method":"server.ping","params":[8,"12345678"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_defaults__empty_data)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":210,"method":"server.ping","params":[]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").at("data").is_string());
    BC_REQUIRE_EQUAL(response.at("result").at("data").as_string(), "");
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_zero_length__empty_data)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":212,"method":"server.ping","params":[0,""]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").at("data").is_string());
    BC_REQUIRE_EQUAL(response.at("result").at("data").as_string(), "");
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_invalid_data_encoding__invalid_argument)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto result = get_error(R"({"id":211,"method":"server.ping","params":[8,"not_hex!"]})" "\n");
    BC_REQUIRE_EQUAL(result, invalid_argument.value());
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_odd_length_data__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":216,"method":"server.ping","params":[2,"abc"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("result").at("data").as_string(), "00");
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_length_under_data__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":213,"method":"server.ping","params":[5,"12345678"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("result").at("data").as_string(), "00000");
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_excessive_length__truncated)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":217,"method":"server.ping","params":[1000000,""]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("result").at("data").as_string().size(), 1024u);
}

BC_AUTO_TEST_CASE(electrum__server_ping__v1_7_data__expected)
{
    BC_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":214,"method":"server.ping","params":[8,"12345678"]})" "\n");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("result").at("data").as_string(), "00000000");
}

BC_AUTO_TEST_SUITE_END()
