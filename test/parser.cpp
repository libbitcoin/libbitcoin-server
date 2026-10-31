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

BC_FIXTURE_TEST_SUITE(parser_tests, test::directory_setup_fixture)

using namespace bc::system::chain;

static const server::settings::embedded_pages pages{};

static std::string write_config(const std::string& text)
{
    const auto path = TEST_PATH + ".cfg";
    std::ofstream file{ path };
    file << "[database]\npath = " << TEST_PATH << "\n" << text;
    return path;
}

static system::config::path store_path(const std::string& network)
{
    return std::filesystem::path{ TEST_PATH } / network;
}

// net

BC_AUTO_TEST_CASE(parser__parse__net_testnet4__testnet4_defaults)
{
    const char* argv[]{ "bs", "-n", "testnet4" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));

    const auto& configured = instance.configured;
    BC_REQUIRE(configured.net == selection::testnet4);
    BC_REQUIRE(!configured.make);
    BC_REQUIRE(configured.bitcoin.network == selection::testnet4);
    BC_REQUIRE(configured.node.network == selection::testnet4);
    BC_REQUIRE_EQUAL(configured.network.identifier, 675223068u);
    BC_REQUIRE(configured.server.network == selection::testnet4);
    BC_REQUIRE_EQUAL(configured.database.path, "testnet4");
    BC_REQUIRE_EQUAL(configured.database.header.expected, 156'456u);
    BC_REQUIRE_EQUAL(configured.database.strong.buckets, 14'810'011u);
}

BC_AUTO_TEST_CASE(parser__parse__net_attached_signet__signet_defaults)
{
    const char* argv[]{ "bs", "-nsignet" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(2, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::signet);
    BC_REQUIRE_EQUAL(instance.configured.network.identifier, 1087308554u);
}

BC_AUTO_TEST_CASE(parser__parse__net_long_form__regtest_defaults)
{
    const char* argv[]{ "bs", "--net=regtest" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(2, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::regtest);
    BC_REQUIRE_EQUAL(instance.configured.server.wallet.witness_prefix, "bcrt");
}

BC_AUTO_TEST_CASE(parser__parse__net_bare__false)
{
    const char* argv[]{ "bs", "-n" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(!instance.parse(2, argv, error));
}

BC_AUTO_TEST_CASE(parser__parse__net_unknown__false)
{
    const char* argv[]{ "bs", "-n", "testnet" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(!instance.parse(3, argv, error));
}

// make

BC_AUTO_TEST_CASE(parser__parse__make__mainnet_defaults)
{
    const char* argv[]{ "bs", "-m" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(2, argv, error));

    const auto& configured = instance.configured;
    BC_REQUIRE(configured.make);
    BC_REQUIRE(configured.net == selection::none);
    BC_REQUIRE(configured.bitcoin.network == selection::mainnet);
    BC_REQUIRE_EQUAL(configured.network.identifier, 3652501241u);
    BC_REQUIRE_EQUAL(configured.database.path, "mainnet");
    BC_REQUIRE_EQUAL(configured.database.strong.buckets, 1'359'871'695u);
}

BC_AUTO_TEST_CASE(parser__parse__make_net__net_defaults)
{
    const char* argv[]{ "bs", "-m", "-n", "regtest" };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(4, argv, error));

    const auto& configured = instance.configured;
    BC_REQUIRE(configured.make);
    BC_REQUIRE(configured.bitcoin.network == selection::regtest);
    BC_REQUIRE_EQUAL(configured.database.path, "regtest");
}

// bitcoin.network

BC_AUTO_TEST_CASE(parser__parse__no_net__unset)
{
    const auto config = write_config("");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    BC_REQUIRE(instance.configured.net == selection::none);
    BC_REQUIRE(!instance.configured.make);
    BC_REQUIRE(instance.configured.bitcoin.network == selection::mainnet);
}

BC_AUTO_TEST_CASE(parser__parse__configured_network__configured_defaults)
{
    const auto config = write_config("[bitcoin]\nnetwork = signet\n");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::signet);
    BC_REQUIRE_EQUAL(instance.configured.network.identifier, 1087308554u);
}

BC_AUTO_TEST_CASE(parser__parse__configured_network_unconfigured_path__network_path)
{
    const auto path = TEST_PATH + ".cfg";
    std::ofstream file{ path };
    file << "[bitcoin]\nnetwork = signet\n";
    file.close();
    const char* argv[]{ "bs", "-c", path.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    BC_REQUIRE_EQUAL(instance.configured.database.path, "signet");
}

BC_AUTO_TEST_CASE(parser__parse__make_configured_network__configured_defaults)
{
    const auto config = write_config("[bitcoin]\nnetwork = regtest\n");
    const char* argv[]{ "bs", "-m", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(4, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::regtest);
    BC_REQUIRE_EQUAL(instance.configured.network.identifier, 3669344250u);
}

BC_AUTO_TEST_CASE(parser__parse__net_matching_network__net_network)
{
    const auto config = write_config("[bitcoin]\nnetwork = testnet4\n");
    const char* argv[]{ "bs", "-n", "testnet4", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(5, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::testnet4);
}

BC_AUTO_TEST_CASE(parser__parse__net_conflicting_network__net_network_configured_values)
{
    const auto config = write_config("[bitcoin]\nnetwork = regtest\n[outbound]\nconnections = 7\n[table.strong]\nbuckets = 42\n");
    const char* argv[]{ "bs", "-n", "testnet4", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(5, argv, error));

    const auto& configured = instance.configured;
    BC_REQUIRE(configured.bitcoin.network == selection::testnet4);
    BC_REQUIRE_EQUAL(configured.network.identifier, 675223068u);
    BC_REQUIRE_EQUAL(configured.network.outbound.connections, 7u);
    BC_REQUIRE_EQUAL(configured.database.path, store_path("testnet4"));
    BC_REQUIRE_EQUAL(configured.database.strong.buckets, 42u);
    BC_REQUIRE(instance.variables().at("bitcoin.network").as<system::config::selection>() == selection::regtest);
}

BC_AUTO_TEST_CASE(parser__parse__net_mainnet_configured_network__configured_defaults)
{
    const auto config = write_config("[bitcoin]\nnetwork = testnet4\n");
    const char* argv[]{ "bs", "-n", "mainnet", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(5, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::testnet4);
    BC_REQUIRE_EQUAL(instance.configured.network.identifier, 675223068u);
}

BC_AUTO_TEST_CASE(parser__parse__configured_path__network_under_path)
{
    const auto config = write_config("");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    BC_REQUIRE_EQUAL(instance.configured.database.path, store_path("mainnet"));
}

BC_AUTO_TEST_CASE(parser__parse__configured_path_net__net_under_path)
{
    const auto config = write_config("[bitcoin]\nnetwork = signet\n");
    const char* argv[]{ "bs", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(5, argv, error));
    BC_REQUIRE_EQUAL(instance.configured.database.path, store_path("regtest"));
}

BC_AUTO_TEST_CASE(parser__parse__configured_path_configured_network__network_under_path)
{
    const auto config = write_config("[bitcoin]\nnetwork = signet\n");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    BC_REQUIRE_EQUAL(instance.configured.database.path, store_path("signet"));
}

// read_envelope

BC_AUTO_TEST_CASE(parser__read_envelope__no_store__unchanged)
{
    const auto config = write_config("");
    const char* argv[]{ "bs", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    instance.read_envelope();
    BC_REQUIRE(instance.stored_code);
    BC_REQUIRE(instance.configured.bitcoin.network == selection::mainnet);
}

BC_AUTO_TEST_CASE(parser__read_envelope__regtest_store__stored_defaults)
{
    const auto config = write_config("");
    const char* create_argv[]{ "bs", "-m", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser creator{ selection::mainnet, pages, pages };
    BC_REQUIRE(creator.parse(6, create_argv, error));
    BC_REQUIRE(!creator.configured.initialize());
    creator.configured.database.path = store_path("mainnet");
    BC_REQUIRE(test::clear(creator.configured.database.path));
    server::server_node::store store{ creator.configured.database };
    BC_REQUIRE(!store.create([](auto, auto) {}));
    BC_REQUIRE(!store.close([](auto, auto) {}));

    const char* argv[]{ "bs", "-c", config.c_str() };
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(3, argv, error));
    BC_REQUIRE(instance.configured.bitcoin.network == selection::mainnet);
    instance.read_envelope();

    BC_REQUIRE(!instance.stored_code);
    BC_REQUIRE(instance.stored.schema == database::envelope::compiled);
    BC_REQUIRE(instance.stored.network == selection::regtest);

    const auto& configured = instance.configured;
    BC_REQUIRE(configured.bitcoin.network == selection::regtest);
    BC_REQUIRE_EQUAL(configured.network.identifier, 3669344250u);
    BC_REQUIRE_EQUAL(configured.database.strong.buckets, 100'000u);
    BC_REQUIRE_EQUAL(configured.database.path, store_path("mainnet"));
}

BC_AUTO_TEST_CASE(parser__read_envelope__specified_network__stored_defaults_specified_network)
{
    const auto config = write_config("");
    const char* create_argv[]{ "bs", "-m", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser creator{ selection::mainnet, pages, pages };
    BC_REQUIRE(creator.parse(6, create_argv, error));
    BC_REQUIRE(!creator.configured.initialize());
    creator.configured.database.path = store_path("testnet4");
    BC_REQUIRE(test::clear(creator.configured.database.path));
    server::server_node::store store{ creator.configured.database };
    BC_REQUIRE(!store.create([](auto, auto) {}));
    BC_REQUIRE(!store.close([](auto, auto) {}));

    const char* argv[]{ "bs", "-n", "testnet4", "-c", config.c_str() };
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(5, argv, error));
    instance.read_envelope();

    BC_REQUIRE(!instance.stored_code);
    BC_REQUIRE(instance.stored.network == selection::regtest);

    const auto& configured = instance.configured;
    BC_REQUIRE(configured.bitcoin.network == selection::testnet4);
    BC_REQUIRE_EQUAL(configured.network.identifier, 3669344250u);
    BC_REQUIRE_EQUAL(configured.database.strong.buckets, 100'000u);
}

BC_AUTO_TEST_CASE(parser__read_envelope__make__unchanged)
{
    const auto config = write_config("");
    const char* create_argv[]{ "bs", "-m", "-n", "regtest", "-c", config.c_str() };
    std::ostringstream error{};
    server::parser creator{ selection::mainnet, pages, pages };
    BC_REQUIRE(creator.parse(6, create_argv, error));
    BC_REQUIRE(!creator.configured.initialize());
    BC_REQUIRE(test::clear(creator.configured.database.path));
    server::server_node::store store{ creator.configured.database };
    BC_REQUIRE(!store.create([](auto, auto) {}));
    BC_REQUIRE(!store.close([](auto, auto) {}));

    const char* argv[]{ "bs", "-m", "-n", "testnet4", "-c", config.c_str() };
    server::parser instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.parse(6, argv, error));
    instance.read_envelope();
    BC_REQUIRE(instance.stored.network == selection::none);
    BC_REQUIRE(instance.configured.bitcoin.network == selection::testnet4);
}

BC_AUTO_TEST_SUITE_END()
