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
static const code target_overflow{ server::error::target_overflow };
static const code invalid_argument{ server::error::invalid_argument };

// server.add_peer

// server.banner

BOOST_AUTO_TEST_CASE(electrum__server_banner__jsonrpc_unspecified_no_aparams__dropped)
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
    BOOST_REQUIRE(handshake(electrum::version::v1_2));

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
    const auto genesis_hash = encode_hash(genesis.hash());
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

BOOST_AUTO_TEST_CASE(electrum__server_features__hosts__expected)
{
    config_.server.electrum.advertise_binds.emplace_back("tcp.com:80");
    config_.server.electrum.advertise_safes.emplace_back("ssl.com:443");
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
