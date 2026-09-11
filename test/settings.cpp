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

BOOST_AUTO_TEST_SUITE(settings_tests)

using namespace bc::network;
using namespace bc::system::chain;
using version = system::config::version;
using namespace bc::system::wallet;

// [log]

BOOST_AUTO_TEST_CASE(settings__log__default_context__expected)
{
    const log::settings log{};
    BOOST_REQUIRE_EQUAL(log.application, levels::application_defined);
    BOOST_REQUIRE_EQUAL(log.news, levels::news_defined);
    BOOST_REQUIRE_EQUAL(log.session, levels::session_defined);
    BOOST_REQUIRE_EQUAL(log.protocol, false /*levels::protocol_defined*/);
    BOOST_REQUIRE_EQUAL(log.proxy, false /*levels::proxy_defined*/);
    BOOST_REQUIRE_EQUAL(log.remote, levels::remote_defined);
    BOOST_REQUIRE_EQUAL(log.fault, levels::fault_defined);
    BOOST_REQUIRE_EQUAL(log.quitting, false /*levels::quitting_defined*/);
    BOOST_REQUIRE_EQUAL(log.objects, false /*levels::objects_defined*/);
    BOOST_REQUIRE_EQUAL(log.verbose, false /*levels::verbose_defined*/);
    BOOST_REQUIRE_EQUAL(log.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(log.path, "");
    BOOST_REQUIRE_EQUAL(log.log_file1(), "bs_end.log");
    BOOST_REQUIRE_EQUAL(log.log_file2(), "bs_begin.log");
    BOOST_REQUIRE_EQUAL(log.events_file(), "events.log");
#if defined(HAVE_MSC)
    BOOST_REQUIRE_EQUAL(log.symbols, "");
#endif
}

// [server]

BOOST_AUTO_TEST_CASE(server__html_server__defaults__expected)
{
    const auto undefined = server::settings::embedded_pages{};
    const server::settings::html_server instance{ "test", undefined };

    // tcp_server
    BOOST_REQUIRE_EQUAL(instance.name, "test");
    BOOST_REQUIRE(instance.binds.empty());
    BOOST_REQUIRE_EQUAL(instance.connections, 0u);
    BOOST_REQUIRE_EQUAL(instance.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(instance.expiration_minutes, 60u);
    BOOST_REQUIRE(!instance.enabled());
    BOOST_REQUIRE(instance.inactivity() == minutes(10));
    BOOST_REQUIRE(instance.expiration() == minutes(60));

    // tls_server
    BOOST_REQUIRE(!instance.secure());
    BOOST_REQUIRE(instance.safes.empty());
    BOOST_REQUIRE(instance.cert_auth.empty());
    BOOST_REQUIRE(instance.cert_path.empty());
    BOOST_REQUIRE(instance.key_path.empty());
    BOOST_REQUIRE(instance.key_pass.empty());

    // http_server
    BOOST_REQUIRE_EQUAL(instance.server, BC_HTTP_SERVER_NAME);
    BOOST_REQUIRE(instance.hosts.empty());
    BOOST_REQUIRE(instance.host_names().empty());

    // html_server
    BOOST_REQUIRE(!instance.pages.enabled());
    BOOST_REQUIRE(instance.pages.css().empty());
    BOOST_REQUIRE(instance.pages.html().empty());
    BOOST_REQUIRE(instance.pages.ecma().empty());
    BOOST_REQUIRE(instance.pages.font().empty());
    BOOST_REQUIRE(instance.pages.icon().empty());
    BOOST_REQUIRE(instance.websocket);
    BOOST_REQUIRE(instance.path.empty());
    BOOST_REQUIRE_EQUAL(instance.default_, "index.html");
}

BOOST_AUTO_TEST_CASE(server__admin_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.admin;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "admin");
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BOOST_REQUIRE(!server.enabled());
    BOOST_REQUIRE(server.inactivity() == minutes(10));
    BOOST_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BOOST_REQUIRE(!server.secure());
    BOOST_REQUIRE(server.safes.empty());
    BOOST_REQUIRE(server.cert_auth.empty());
    BOOST_REQUIRE(server.cert_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_pass.empty());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());

    // html_server
    BOOST_REQUIRE(!server.pages.enabled());
    BOOST_REQUIRE(server.pages.css().empty());
    BOOST_REQUIRE(server.pages.html().empty());
    BOOST_REQUIRE(server.pages.ecma().empty());
    BOOST_REQUIRE(server.pages.font().empty());
    BOOST_REQUIRE(server.pages.icon().empty());
    BOOST_REQUIRE(server.path.empty());
    BOOST_REQUIRE(server.websocket);
    BOOST_REQUIRE_EQUAL(server.default_, "index.html");
}

BOOST_AUTO_TEST_CASE(server__native_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.native;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "native");
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BOOST_REQUIRE(!server.enabled());
    BOOST_REQUIRE(server.inactivity() == minutes(10));
    BOOST_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BOOST_REQUIRE(!server.secure());
    BOOST_REQUIRE(server.safes.empty());
    BOOST_REQUIRE(server.cert_auth.empty());
    BOOST_REQUIRE(server.cert_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_pass.empty());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());

    // html_server
    BOOST_REQUIRE(!server.pages.enabled());
    BOOST_REQUIRE(server.pages.css().empty());
    BOOST_REQUIRE(server.pages.html().empty());
    BOOST_REQUIRE(server.pages.ecma().empty());
    BOOST_REQUIRE(server.pages.font().empty());
    BOOST_REQUIRE(server.pages.icon().empty());
    BOOST_REQUIRE(server.path.empty());
    BOOST_REQUIRE(server.websocket);
    BOOST_REQUIRE_EQUAL(server.default_, "index.html");
}

// TODO: could add websocket under bitcoind as a custom property.
BOOST_AUTO_TEST_CASE(server__bitcoind_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.bitcoind;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "bitcoind");
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BOOST_REQUIRE(!server.enabled());
    BOOST_REQUIRE(server.inactivity() == minutes(10));
    BOOST_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BOOST_REQUIRE(!server.secure());
    BOOST_REQUIRE(server.safes.empty());
    BOOST_REQUIRE(server.cert_auth.empty());
    BOOST_REQUIRE(server.cert_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_pass.empty());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());
}

BOOST_AUTO_TEST_CASE(server__electrum_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.electrum;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "electrum");
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BOOST_REQUIRE(!server.enabled());
    BOOST_REQUIRE(server.inactivity() == minutes(10));
    BOOST_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BOOST_REQUIRE(!server.secure());
    BOOST_REQUIRE(server.safes.empty());
    BOOST_REQUIRE(server.cert_auth.empty());
    BOOST_REQUIRE(server.cert_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_pass.empty());

    // electrum_server
    BOOST_REQUIRE_EQUAL(server.maximum_headers, 10u * 2016u);
    BOOST_REQUIRE_EQUAL(server.maximum_history, 1'000'000u);
    BOOST_REQUIRE_EQUAL(server.maximum_subscriptions, 1'000'000u);
    BOOST_REQUIRE_EQUAL(server.protocol_minimum, version(1, 0, 0, 0));
    BOOST_REQUIRE_EQUAL(server.protocol_maximum, version(1, 7, 0, 0));
    BOOST_REQUIRE_EQUAL(server.server_name, BC_USER_AGENT);
    BOOST_REQUIRE(server.donation_address.empty());
    BOOST_REQUIRE(server.banner_message.empty());
    BOOST_REQUIRE(server.self_binds.empty());
    BOOST_REQUIRE(server.self_safes.empty());
    BOOST_REQUIRE(server.more_binds.empty());
    BOOST_REQUIRE(server.more_safes.empty());
}

BOOST_AUTO_TEST_CASE(server__stratum_v1_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.stratum_v1;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "stratum_v1");
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BOOST_REQUIRE(!server.enabled());
    BOOST_REQUIRE(server.inactivity() == minutes(10));
    BOOST_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BOOST_REQUIRE(!server.secure());
    BOOST_REQUIRE(server.safes.empty());
    BOOST_REQUIRE(server.cert_auth.empty());
    BOOST_REQUIRE(server.cert_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_pass.empty());
}

BOOST_AUTO_TEST_CASE(server__stratum_v2_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.stratum_v2;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "stratum_v2");
    BOOST_REQUIRE(server.binds.empty());
    BOOST_REQUIRE_EQUAL(server.connections, 0u);
    BOOST_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BOOST_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BOOST_REQUIRE(!server.enabled());
    BOOST_REQUIRE(server.inactivity() == minutes(10));
    BOOST_REQUIRE(server.expiration() == minutes(60));
}


// [wallet]

#define MAINNET_M "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi"
#define TESTNET_M "tprv8ZgxMBicQKsPeDgjzdC36fs6bMjGApWDNLR9erAXMs5skhMv36j9MV5ecvfavji5khqjWaWSFhN3YcCUUdiKH6isR4Pwy3U5y5egddBr16m"

BOOST_AUTO_TEST_CASE(wallet__defaults__mainnet__expected)
{
    const server::settings::wallet_settings instance{ selection::mainnet };
    BOOST_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::main::btc);
    BOOST_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::main::btc);
    BOOST_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::main::btc);
    BOOST_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::main::btc);
    BOOST_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::main::btc.prv);
    BOOST_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::main::btc.pub);
}

BOOST_AUTO_TEST_CASE(wallet__defaults__testnet__expected)
{
    const server::settings::wallet_settings instance{ selection::testnet3 };
    BOOST_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::test::btc);
    BOOST_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::test::btc);
    BOOST_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::test::btc);
    BOOST_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::test::btc);
    BOOST_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::test::btc.prv);
    BOOST_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::test::btc.pub);
}

BOOST_AUTO_TEST_CASE(wallet__defaults__regtest__testnet_versions_with_regtest_witness)
{
    const server::settings::wallet_settings instance{ selection::regtest };
    BOOST_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::test::btc);
    BOOST_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::test::btc);
    BOOST_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::test::btc);
    BOOST_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::regtest::btc);
    BOOST_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::test::btc.prv);
}

BOOST_AUTO_TEST_CASE(wallet__to_context__mainnet__matches_predefined)
{
    const server::settings::wallet_settings instance{ selection::mainnet };
    BOOST_REQUIRE_EQUAL(instance.to_context().hd_prefixes(), ctx::btc::main.hd_prefixes());
    BOOST_REQUIRE_EQUAL(instance.to_context().versions(), ctx::btc::main.versions());
    BOOST_REQUIRE_EQUAL(instance.to_context().p2w, ctx::btc::main.p2w);
}

BOOST_AUTO_TEST_CASE(wallet__to_context__regtest__matches_predefined)
{
    const server::settings::wallet_settings instance{ selection::regtest };
    BOOST_REQUIRE_EQUAL(instance.to_context().hd_prefixes(), ctx::btc::regtest.hd_prefixes());
    BOOST_REQUIRE_EQUAL(instance.to_context().versions(), ctx::btc::regtest.versions());
    BOOST_REQUIRE_EQUAL(instance.to_context().p2w, ctx::btc::regtest.p2w);
}

BOOST_AUTO_TEST_CASE(wallet__to_context__testnet__parses_testnet_descriptor_only)
{
    const server::settings::wallet_settings instance{ selection::testnet3 };
    BOOST_REQUIRE(descriptor("pkh(" TESTNET_M "/1/*)", instance.to_context()));
    BOOST_REQUIRE(!descriptor("pkh(" MAINNET_M "/1/*)", instance.to_context()));
}

BOOST_AUTO_TEST_CASE(wallet__to_context__mainnet__parses_mainnet_descriptor_only)
{
    const server::settings::wallet_settings instance{ selection::mainnet };
    BOOST_REQUIRE(descriptor("pkh(" MAINNET_M "/1/*)", instance.to_context()));
    BOOST_REQUIRE(!descriptor("pkh(" TESTNET_M "/1/*)", instance.to_context()));
}

BOOST_AUTO_TEST_CASE(wallet__to_context__configured_prefixes__override_network)
{
    server::settings::wallet_settings instance{ selection::mainnet };
    instance.hd_private_prefix = prefix::hd::test::btc.prv;
    instance.hd_public_prefix = prefix::hd::test::btc.pub;
    BOOST_REQUIRE(descriptor("pkh(" TESTNET_M "/1/*)", instance.to_context()));
}
BOOST_AUTO_TEST_SUITE_END()
