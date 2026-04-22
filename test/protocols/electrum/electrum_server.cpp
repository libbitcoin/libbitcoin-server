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

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

using namespace system;
static const code wrong_version{ server::error::wrong_version };
static const code not_implemented{ server::error::not_implemented };

// server.add_peer

BOOST_AUTO_TEST_CASE(electrum__server_add_peer__insufficient_version__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":400,"method":"server.add_peer","params":[{}]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), wrong_version.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_add_peer__v1_1__not_implemented)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto response = get(R"({"id":401,"method":"server.add_peer","params":[{}]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("error").as_object().at("code").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("error").as_object().at("code").as_int64(), not_implemented.value());
}

// server.banner

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_no_params__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    // params[] required in json 1.0, server drops connection for invalid json-rpc.
    const auto response = get(R"({"id":42,"method":"server.banner"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_named_params__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    // params{} disallowed in json 1.0, server drops connection for invalid json-rpc.
    const auto response = get(R"({"id":42,"method":"server.banner","params":{}})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_empty_params__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":42,"method":"server.banner","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_1__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"1.0","id":42,"method":"server.banner","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_2__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"2.0","id":42,"method":"server.banner"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "banner_message");
}

// server.donation_address

BOOST_AUTO_TEST_CASE(electrum__server_donation_address__jsonrpc_1__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"1.0","id":43,"method":"server.donation_address","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "donation_address");
}

BOOST_AUTO_TEST_CASE(electrum__server_donation_address__jsonrpc_2__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"jsonrpc":"2.0","id":43,"method":"server.donation_address"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "donation_address");
}

// server.features

BOOST_AUTO_TEST_CASE(electrum__server_features__no_params__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":301,"method":"server.features"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":302,"method":"server.features","params":["extra"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__default_hosts__expected)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("genesis_hash").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("hash_function").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("server_version").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_min").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_max").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("pruning").is_null());
    REQUIRE_NO_THROW_TRUE(result.at("hosts").is_object());

    using namespace electrum;
    const auto min = version_to_string(protocol_electrum_version::minimum);
    const auto max = version_to_string(protocol_electrum_version::maximum);
    const auto server_name = config_.server.electrum.server_name;
    const auto genesis_hash = encode_hash(test::genesis.hash());
    BOOST_REQUIRE_EQUAL(result.at("genesis_hash").as_string(), genesis_hash);
    BOOST_REQUIRE_EQUAL(result.at("hash_function").as_string(), "sha256");
    BOOST_REQUIRE_EQUAL(result.at("server_version").as_string(), server_name);
    BOOST_REQUIRE_EQUAL(result.at("protocol_min").as_string(), min);
    BOOST_REQUIRE_EQUAL(result.at("protocol_max").as_string(), max);

    const auto& hosts = result.at("hosts").as_object();
    BOOST_REQUIRE_EQUAL(hosts.size(), 1u);
    REQUIRE_NO_THROW_TRUE(hosts.at("").is_object());

    const auto& host = hosts.at("").as_object();
    REQUIRE_NO_THROW_TRUE(host.at("tcp_port").is_null());
    REQUIRE_NO_THROW_TRUE(host.at("ssl_port").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__v1_6__hash_function_removed)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE(!result.contains("hash_function"));
    REQUIRE_NO_THROW_TRUE(result.at("genesis_hash").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("server_version").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_min").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("protocol_max").is_string());
    REQUIRE_NO_THROW_TRUE(result.at("pruning").is_null());
    REQUIRE_NO_THROW_TRUE(result.at("hosts").is_object());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__method_flavours_v1_7__supports_verbose_true_false)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("method_flavours").is_object());

    const auto& flavours = result.at("method_flavours").as_object();
    REQUIRE_NO_THROW_TRUE(flavours.at("blockchain.transaction.broadcast_package").is_object());

    const auto& package = flavours.at("blockchain.transaction.broadcast_package").as_object();
    REQUIRE_NO_THROW_TRUE(package.at("supports_verbose_true").is_bool());
    BOOST_REQUIRE(!package.at("supports_verbose_true").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__hosts__expected)
{
    config_.server.electrum.self_binds.emplace_back("tcp.com:80");
    config_.server.electrum.self_safes.emplace_back("ssl.com:443");
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":300,"method":"server.features","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("hosts").is_object());

    const auto& hosts = result.at("hosts").as_object();
    REQUIRE_NO_THROW_TRUE(hosts.at("tcp.com").is_object());
    REQUIRE_NO_THROW_TRUE(hosts.at("ssl.com").is_object());

    const auto& tcp = hosts.at("tcp.com").as_object();
    const auto& ssl = hosts.at("ssl.com").as_object();
    BOOST_REQUIRE_EQUAL(tcp.at("tcp_port").as_int64(), 80);
    BOOST_REQUIRE_EQUAL(ssl.at("ssl_port").as_int64(), 443);
}

// server.peers.subscribe

BOOST_AUTO_TEST_CASE(electrum__server_peers_subscribe__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":502,"method":"server.peers.subscribe","params":["extra"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_peers_subscribe__empty_params__empty_array)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":503,"method":"server.peers.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BOOST_REQUIRE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(electrum__server_peers_subscribe__configured_peers__expected)
{
    config_.server.electrum.more_binds.emplace_back("another.trusted.server:50001");
    config_.server.electrum.more_safes.emplace_back("electrum.example.com");
    config_.server.electrum.more_safes.emplace_back("fulcrum.trustednode.org:50002");
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":504,"method":"server.peers.subscribe","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& peers = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(peers.size(), 3u);

    // Peer 0: another.trusted.server:50001
    const auto& peer0 = peers[0].as_array();
    BOOST_REQUIRE_EQUAL(peer0.size(), 3u);
    BOOST_REQUIRE_EQUAL(peer0[0].as_string(), "another.trusted.server");
    BOOST_REQUIRE_EQUAL(peer0[1].as_string(), "another.trusted.server");

    const auto& features0 = peer0[2].as_array();
    BOOST_REQUIRE_EQUAL(features0.size(), 1u);
    BOOST_REQUIRE_EQUAL(features0[0].as_string(), "t50001");

    // Peer 1: electrum.example.com (default SSL)
    const auto& peer1 = peers[1].as_array();
    BOOST_REQUIRE_EQUAL(peer1.size(), 3u);
    BOOST_REQUIRE_EQUAL(peer1[0].as_string(), "electrum.example.com");
    BOOST_REQUIRE_EQUAL(peer1[1].as_string(), "electrum.example.com");

    const auto& features1 = peer1[2].as_array();
    BOOST_REQUIRE_EQUAL(features1.size(), 1u);
    BOOST_REQUIRE_EQUAL(features1[0].as_string(), "s");

    // Peer 2: fulcrum.trustednode.org:50002
    const auto& peer2 = peers[2].as_array();
    BOOST_REQUIRE_EQUAL(peer2.size(), 3u);
    BOOST_REQUIRE_EQUAL(peer2[0].as_string(), "fulcrum.trustednode.org");
    BOOST_REQUIRE_EQUAL(peer2[1].as_string(), "fulcrum.trustednode.org");

    const auto& features2 = peer2[2].as_array();
    BOOST_REQUIRE_EQUAL(features2.size(), 1u);
    BOOST_REQUIRE_EQUAL(features2[0].as_string(), "s50002");
}

// server.ping

BOOST_AUTO_TEST_CASE(electrum__server_ping__always__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":200,"method":"server.ping","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(electrum__server_ping__jsonrpc_unspecified_no_params__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":201,"method":"server.ping"})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__server_ping__extra_param__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

    const auto response = get(R"({"id":202,"method":"server.ping","params":["extra"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()
