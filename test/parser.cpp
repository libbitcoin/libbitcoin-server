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
#include "test.hpp"

#include <fstream>
#include <sstream>

BOOST_FIXTURE_TEST_SUITE(parser_tests, test::directory_setup_fixture)

using namespace bc::system::chain;

static const server::settings::embedded_pages pages{};

static std::string write_config(const std::string& text)
{
    const auto path = TEST_PATH + ".cfg";
    std::ofstream file{ path };
    file << "[database]\npath = " << TEST_PATH << "\n" << text;
    return path;
}

// net

BOOST_AUTO_TEST_CASE(parser__parse__net_testnet4__testnet4_defaults)
{
    const char* argv[]{ "bs", "-n", "testnet4" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(3, argv, error));

    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.net == selection::testnet4);
    BOOST_REQUIRE(!configured.make);
    BOOST_REQUIRE(configured.bitcoin.network == selection::testnet4);
    BOOST_REQUIRE(configured.node.network == selection::testnet4);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, 675223068u);
    BOOST_REQUIRE(configured.server.network == selection::testnet4);
    BOOST_REQUIRE_EQUAL(configured.database.path, "testnet4");
    BOOST_REQUIRE_EQUAL(configured.database.header.expected, 156'456u);
    BOOST_REQUIRE_EQUAL(configured.database.strong.buckets, 14'810'011u);
}

BOOST_AUTO_TEST_CASE(parser__parse__net_attached_signet__signet_defaults)
{
    const char* argv[]{ "bs", "-nsignet" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(2, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::signet);
    BOOST_REQUIRE_EQUAL(instance.configured.network.identifier, 1087308554u);
}

BOOST_AUTO_TEST_CASE(parser__parse__net_long_form__regtest_defaults)
{
    const char* argv[]{ "bs", "--net=regtest" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(2, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::regtest);
    BOOST_REQUIRE_EQUAL(instance.configured.server.wallet.witness_prefix, "bcrt");
}

BOOST_AUTO_TEST_CASE(parser__parse__net_bare__false)
{
    const char* argv[]{ "bs", "-n" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(!instance.parse(2, argv, error));
}

BOOST_AUTO_TEST_CASE(parser__parse__net_unknown__false)
{
    const char* argv[]{ "bs", "-n", "testnet" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(!instance.parse(3, argv, error));
}

// make

BOOST_AUTO_TEST_CASE(parser__parse__make__mainnet_defaults)
{
    const char* argv[]{ "bs", "-m" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(2, argv, error));

    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.make);
    BOOST_REQUIRE(configured.net == selection::none);
    BOOST_REQUIRE(configured.bitcoin.network == selection::mainnet);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, 3652501241u);
    BOOST_REQUIRE_EQUAL(configured.database.path, "mainnet");
    BOOST_REQUIRE_EQUAL(configured.database.strong.buckets, 1'359'871'695u);
}

BOOST_AUTO_TEST_CASE(parser__parse__make_net__net_defaults)
{
    const char* argv[]{ "bs", "-m", "-n", "regtest" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(4, argv, error));

    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.make);
    BOOST_REQUIRE(configured.bitcoin.network == selection::regtest);
    BOOST_REQUIRE_EQUAL(configured.database.path, "regtest");
}

// bitcoin.network

BOOST_AUTO_TEST_CASE(parser__parse__no_net__unset)
{
    const auto config = write_config("");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(3, argv, error));
    BOOST_REQUIRE(instance.configured.net == selection::none);
    BOOST_REQUIRE(!instance.configured.make);
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::mainnet);
}

BOOST_AUTO_TEST_CASE(parser__parse__configured_network__configured_defaults)
{
    const auto config = write_config("[bitcoin]\nnetwork = signet\n");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(3, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::signet);
    BOOST_REQUIRE_EQUAL(instance.configured.network.identifier, 1087308554u);
}

BOOST_AUTO_TEST_CASE(parser__parse__configured_network_unconfigured_path__network_path)
{
    const auto path = TEST_PATH + ".cfg";
    std::ofstream file{ path };
    file << "[bitcoin]\nnetwork = signet\n";
    file.close();
    const char* argv[]{ "bs", "-c", path.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(3, argv, error));
    BOOST_REQUIRE_EQUAL(instance.configured.database.path, "signet");
}

BOOST_AUTO_TEST_CASE(parser__parse__make_configured_network__configured_defaults)
{
    const auto config = write_config("[bitcoin]\nnetwork = regtest\n");
    const char* argv[]{ "bs", "-m", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(4, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::regtest);
    BOOST_REQUIRE_EQUAL(instance.configured.network.identifier, 3669344250u);
}

BOOST_AUTO_TEST_CASE(parser__parse__net_matching_network__net_network)
{
    const auto config = write_config("[bitcoin]\nnetwork = testnet4\n");
    const char* argv[]{ "bs", "-n", "testnet4", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(5, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::testnet4);
}

BOOST_AUTO_TEST_CASE(parser__parse__net_conflicting_network__net_network_configured_values)
{
    const auto config = write_config("[bitcoin]\nnetwork = regtest\n[outbound]\nconnections = 7\n[table.strong]\nbuckets = 42\n");
    const char* argv[]{ "bs", "-n", "testnet4", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(5, argv, error));

    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.bitcoin.network == selection::testnet4);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, 675223068u);
    BOOST_REQUIRE_EQUAL(configured.network.outbound.connections, 7u);
    BOOST_REQUIRE_EQUAL(configured.database.path, TEST_PATH);
    BOOST_REQUIRE_EQUAL(configured.database.strong.buckets, 42u);
    BOOST_REQUIRE(instance.variables().at("bitcoin.network").as<system::config::selection>() == selection::regtest);
}

BOOST_AUTO_TEST_CASE(parser__parse__net_mainnet_configured_network__configured_defaults)
{
    const auto config = write_config("[bitcoin]\nnetwork = testnet4\n");
    const char* argv[]{ "bs", "-n", "mainnet", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(5, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::testnet4);
    BOOST_REQUIRE_EQUAL(instance.configured.network.identifier, 675223068u);
}

// read_envelope

BOOST_AUTO_TEST_CASE(parser__read_envelope__no_store__unchanged)
{
    const auto config = write_config("");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(3, argv, error));
    instance.read_envelope();
    BOOST_REQUIRE(instance.stored_code);
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::mainnet);
}

BOOST_AUTO_TEST_CASE(parser__read_envelope__regtest_store__stored_defaults)
{
    const auto config = write_config("");
    const char* create_argv[]{ "bs", "-m", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser creator{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(creator.parse(6, create_argv, error));
    BOOST_REQUIRE(!creator.configured.initialize());
    BOOST_REQUIRE(test::clear(TEST_PATH));
    server::server_node::store store{ creator.configured.database };
    BOOST_REQUIRE(!store.create([](auto, auto) {}));
    BOOST_REQUIRE(!store.close([](auto, auto) {}));

    const char* argv[]{ "bs", "-c", config.c_str() };
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(3, argv, error));
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::mainnet);
    instance.read_envelope();

    BOOST_REQUIRE(!instance.stored_code);
    BOOST_REQUIRE(instance.stored.schema == database::envelope::compiled);
    BOOST_REQUIRE(instance.stored.network == selection::regtest);

    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.bitcoin.network == selection::regtest);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, 3669344250u);
    BOOST_REQUIRE_EQUAL(configured.database.strong.buckets, 100'000u);
}

BOOST_AUTO_TEST_CASE(parser__read_envelope__specified_network__stored_defaults_specified_network)
{
    const auto config = write_config("");
    const char* create_argv[]{ "bs", "-m", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser creator{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(creator.parse(6, create_argv, error));
    BOOST_REQUIRE(!creator.configured.initialize());
    BOOST_REQUIRE(test::clear(TEST_PATH));
    server::server_node::store store{ creator.configured.database };
    BOOST_REQUIRE(!store.create([](auto, auto) {}));
    BOOST_REQUIRE(!store.close([](auto, auto) {}));

    const char* argv[]{ "bs", "-n", "testnet4", "-c", config.c_str() };
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(5, argv, error));
    instance.read_envelope();

    BOOST_REQUIRE(!instance.stored_code);
    BOOST_REQUIRE(instance.stored.network == selection::regtest);

    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.bitcoin.network == selection::testnet4);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, 3669344250u);
    BOOST_REQUIRE_EQUAL(configured.database.strong.buckets, 100'000u);
}

BOOST_AUTO_TEST_CASE(parser__read_envelope__make__unchanged)
{
    const auto config = write_config("");
    const char* create_argv[]{ "bs", "-m", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser creator{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(creator.parse(6, create_argv, error));
    BOOST_REQUIRE(!creator.configured.initialize());
    BOOST_REQUIRE(test::clear(TEST_PATH));
    server::server_node::store store{ creator.configured.database };
    BOOST_REQUIRE(!store.create([](auto, auto) {}));
    BOOST_REQUIRE(!store.close([](auto, auto) {}));

    const char* argv[]{ "bs", "-m", "-n", "testnet4", "-c", config.c_str() };
    server::parser instance{ selection::mainnet, pages, pages };
    BOOST_REQUIRE(instance.parse(6, argv, error));
    instance.read_envelope();
    BOOST_REQUIRE(instance.stored.network == selection::none);
    BOOST_REQUIRE(instance.configured.bitcoin.network == selection::testnet4);
}

BOOST_AUTO_TEST_SUITE_END()
