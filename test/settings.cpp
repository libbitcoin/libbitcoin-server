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

#include <filesystem>
#include <fstream>

BC_AUTO_TEST_SUITE(settings_tests)

using namespace bc::network;
using namespace bc::system::chain;
using version = system::config::version;
using namespace bc::system::wallet;

// [log]

BC_AUTO_TEST_CASE(settings__log__default_context__expected)
{
    const log::settings log{};
    BC_REQUIRE_EQUAL(log.application, levels::application_defined);
    BC_REQUIRE_EQUAL(log.news, levels::news_defined);
    BC_REQUIRE_EQUAL(log.session, levels::session_defined);
    BC_REQUIRE_EQUAL(log.protocol, false /*levels::protocol_defined*/);
    BC_REQUIRE_EQUAL(log.proxy, false /*levels::proxy_defined*/);
    BC_REQUIRE_EQUAL(log.remote, levels::remote_defined);
    BC_REQUIRE_EQUAL(log.fault, levels::fault_defined);
    BC_REQUIRE_EQUAL(log.quitting, false /*levels::quitting_defined*/);
    BC_REQUIRE_EQUAL(log.objects, false /*levels::objects_defined*/);
    BC_REQUIRE_EQUAL(log.verbose, false /*levels::verbose_defined*/);
    BC_REQUIRE_EQUAL(log.console, true);
    BC_REQUIRE_EQUAL(log.events, true);
    BC_REQUIRE_EQUAL(log.thread_priority, false);
    BC_REQUIRE(log.thread_priority_() == network::processing_priority::low);
    BC_REQUIRE_EQUAL(log.limit, 1'000'000_u32);
    BC_REQUIRE_EQUAL(log.console_path, "");
    BC_REQUIRE_EQUAL(log.events_path, "");
    BC_REQUIRE_EQUAL(log.log_file1(), "console_new.log");
    BC_REQUIRE_EQUAL(log.log_file2(), "console_old.log");
    BC_REQUIRE_EQUAL(log.events_file(), "events.log");
#if defined(HAVE_WINDOWS)
    BC_REQUIRE_EQUAL(log.symbols_path, "symbols");
#endif
}

// [server]

BC_AUTO_TEST_CASE(server__html_server__defaults__expected)
{
    const auto undefined = server::settings::embedded_pages{};
    const server::settings::html_server instance{ "test", undefined };

    // tcp_server
    BC_REQUIRE_EQUAL(instance.name, "test");
    BC_REQUIRE(instance.binds.empty());
    BC_REQUIRE_EQUAL(instance.connections, 0u);
    BC_REQUIRE_EQUAL(instance.inactivity_minutes, 10u);
    BC_REQUIRE_EQUAL(instance.expiration_minutes, 60u);
    BC_REQUIRE(!instance.enabled());
    BC_REQUIRE(instance.inactivity() == minutes(10));
    BC_REQUIRE(instance.expiration() == minutes(60));

    // tls_server
    BC_REQUIRE(!instance.secure());
    BC_REQUIRE(instance.safes.empty());
    BC_REQUIRE(instance.cert_auth.empty());
    BC_REQUIRE(instance.cert_path.empty());
    BC_REQUIRE(instance.key_path.empty());
    BC_REQUIRE(instance.key_pass.empty());

    // http_server
    BC_REQUIRE_EQUAL(instance.server, BC_HTTP_SERVER_NAME);
    BC_REQUIRE(instance.hosts.empty());
    BC_REQUIRE(instance.host_names().empty());

    // html_server
    BC_REQUIRE(!instance.pages.enabled());
    BC_REQUIRE(instance.pages.css().empty());
    BC_REQUIRE(instance.pages.html().empty());
    BC_REQUIRE(instance.pages.ecma().empty());
    BC_REQUIRE(instance.pages.font().empty());
    BC_REQUIRE(instance.pages.icon().empty());
    BC_REQUIRE(instance.websocket);
    BC_REQUIRE(instance.path.empty());
    BC_REQUIRE_EQUAL(instance.default_, "index.html");
}

BC_AUTO_TEST_CASE(server__settings__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    BC_REQUIRE(instance.path.empty());
}

BC_AUTO_TEST_CASE(server__admin_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.admin;

    // tcp_server
    BC_REQUIRE_EQUAL(server.name, "admin");
    BC_REQUIRE_EQUAL(server.binds.size(), 1u);
    BC_REQUIRE_EQUAL(server.binds.front().ip(), system::ip::address{ system::ip::ipv4::loopback() });
    BC_REQUIRE_EQUAL(server.binds.front().port(), 0u);
    BC_REQUIRE_EQUAL(server.connections, 10u);
    BC_REQUIRE_EQUAL(server.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(server.expiration_minutes, 0u);
    BC_REQUIRE(server.enabled());
    BC_REQUIRE(server.inactivity() == minutes(0));
    BC_REQUIRE(server.expiration() == minutes(0));

    // tls_server
    BC_REQUIRE(!server.secure());
    BC_REQUIRE(server.safes.empty());
    BC_REQUIRE(server.cert_auth.empty());
    BC_REQUIRE(server.cert_path.empty());
    BC_REQUIRE(server.key_path.empty());
    BC_REQUIRE(server.key_pass.empty());

    // http_server
    BC_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BC_REQUIRE(server.hosts.empty());
    BC_REQUIRE(server.host_names().empty());

    // html_server
    BC_REQUIRE(!server.pages.enabled());
    BC_REQUIRE(server.pages.css().empty());
    BC_REQUIRE(server.pages.html().empty());
    BC_REQUIRE(server.pages.ecma().empty());
    BC_REQUIRE(server.pages.font().empty());
    BC_REQUIRE(server.pages.icon().empty());
    BC_REQUIRE(server.path.empty());
    BC_REQUIRE(server.websocket);
    BC_REQUIRE_EQUAL(server.default_, "index.html");

    // native_server
    BC_REQUIRE_EQUAL(server.maximum_history, 1'000'000u);
}

BC_AUTO_TEST_CASE(server__native_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.native;

    // tcp_server
    BC_REQUIRE_EQUAL(server.name, "native");
    BC_REQUIRE(server.binds.empty());
    BC_REQUIRE_EQUAL(server.connections, 0u);
    BC_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BC_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BC_REQUIRE(!server.enabled());
    BC_REQUIRE(server.inactivity() == minutes(10));
    BC_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BC_REQUIRE(!server.secure());
    BC_REQUIRE(server.safes.empty());
    BC_REQUIRE(server.cert_auth.empty());
    BC_REQUIRE(server.cert_path.empty());
    BC_REQUIRE(server.key_path.empty());
    BC_REQUIRE(server.key_pass.empty());

    // http_server
    BC_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BC_REQUIRE(server.hosts.empty());
    BC_REQUIRE(server.host_names().empty());

    // html_server
    BC_REQUIRE(!server.pages.enabled());
    BC_REQUIRE(server.pages.css().empty());
    BC_REQUIRE(server.pages.html().empty());
    BC_REQUIRE(server.pages.ecma().empty());
    BC_REQUIRE(server.pages.font().empty());
    BC_REQUIRE(server.pages.icon().empty());
    BC_REQUIRE(server.path.empty());
    BC_REQUIRE(server.websocket);
    BC_REQUIRE_EQUAL(server.default_, "index.html");

    // native_server
    BC_REQUIRE_EQUAL(server.maximum_history, 1'000'000u);
}

// TODO: could add websocket under bitcoind as a custom property.
BC_AUTO_TEST_CASE(server__bitcoind_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.bitcoind;

    // tcp_server
    BC_REQUIRE_EQUAL(server.name, "bitcoind");
    BC_REQUIRE(server.binds.empty());
    BC_REQUIRE_EQUAL(server.connections, 0u);
    BC_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BC_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BC_REQUIRE(!server.enabled());
    BC_REQUIRE(server.inactivity() == minutes(10));
    BC_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BC_REQUIRE(!server.secure());
    BC_REQUIRE(server.safes.empty());
    BC_REQUIRE(server.cert_auth.empty());
    BC_REQUIRE(server.cert_path.empty());
    BC_REQUIRE(server.key_path.empty());
    BC_REQUIRE(server.key_pass.empty());

    // http_server
    BC_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BC_REQUIRE(server.hosts.empty());
    BC_REQUIRE(server.host_names().empty());

    // bitcoind_server
    BC_REQUIRE_EQUAL(server.protocol_minimum, 300000u);
    BC_REQUIRE_EQUAL(server.protocol_maximum, 320000u);
    BC_REQUIRE(!server.enable_utxo);
    BC_REQUIRE_EQUAL(server.maximum_history, 1'000'000u);
}

BC_AUTO_TEST_CASE(server__bitcoind_server__initialize_context_defaults__success)
{
    server::settings::bitcoind_server instance{ "bitcoind" };
    BC_REQUIRE(!instance.initialize_context());
    BC_REQUIRE_EQUAL(instance.inactivity_minutes, 10u);
    BC_REQUIRE_EQUAL(instance.expiration_minutes, 60u);
}

BC_AUTO_TEST_CASE(server__bitcoind_server__initialize_context_enable_utxo__no_timeouts)
{
    server::settings::bitcoind_server instance{ "bitcoind" };
    instance.enable_utxo = true;
    BC_REQUIRE(!instance.initialize_context());
    BC_REQUIRE_EQUAL(instance.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(instance.expiration_minutes, 0u);
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__initialize_context_defaults__success)
{
    server::settings::stratum_v1_server instance{ "stratum_v1" };
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__initialize_context_small_extranonce1__invalid_configuration)
{
    server::settings::stratum_v1_server instance{ "stratum_v1" };
    instance.extranonce1_size = 3;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__initialize_context_zero_extranonce2__invalid_configuration)
{
    server::settings::stratum_v1_server instance{ "stratum_v1" };
    instance.extranonce2_size = 0;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__initialize_context_oversized_tag__invalid_configuration)
{
    server::settings::stratum_v1_server instance{ "stratum_v1" };
    instance.coinbase_tag = std::string(76, 'x');
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__initialize_context_credential__success)
{
    server::settings::stratum_v1_server instance{ "stratum_v1" };
    instance.credentials = { { "user:pass" } };
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__initialize_context_credential_methods__invalid_configuration)
{
    server::settings::stratum_v1_server instance{ "stratum_v1" };
    instance.credentials = { { "user:pass:mining.submit" } };
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_credential_methods__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.credentials = { { "user:pass:mining" } };
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__coinbase_extranonce_size__defaults__eight)
{
    const server::settings::stratum_v1_server instance{ "stratum_v1" };
    BC_REQUIRE_EQUAL(instance.coinbase_extranonce_size(), 8u);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_defaults__success)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_small_prefix__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.extranonce_prefix_size = 3;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_maximum_extranonce__success)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.extranonce_prefix_size = 24;
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_extranonce_above_maximum__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.extranonce_prefix_size = 25;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_zero_extranonce__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.extranonce_size = 0;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_oversized_extranonce__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.extranonce_size = 72;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_zero_difficulty__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.minimum_difficulty = 0.0;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_nan_difficulty__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.minimum_difficulty = std::numeric_limits<double>::quiet_NaN();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_start_difficulty__success)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.start_difficulty = 42.0;
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_negative_start_difficulty__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.start_difficulty = -1.0;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_infinite_start_difficulty__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.start_difficulty = std::numeric_limits<double>::infinity();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_zero_channels__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.maximum_channels = 0;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__coinbase_extranonce_size__defaults__twelve)
{
    const server::settings::stratum_v2_server instance{ "stratum_v2" };
    BC_REQUIRE_EQUAL(instance.coinbase_extranonce_size(), 12u);
}

// The sri key vector (secret and its public key) and a sri certificate.
static system::config::base58check noise_secret()
{
    return { "zmBEmPhqo3A92FkiLVvyCz6htc3e53ph3ZbD4ASqGaLjwnFLi" };
}

static system::config::base58check noise_secret_key()
{
    return { "9bDuixKmZqAJnrmP746n8zU1wyAQRrus7th9dxnkPg6RzQvCnan" };
}

static system::config::base58check noise_certificate()
{
    return
    {
        "11Bq97W2YjA6HzQsudhPAFE93sqF6b23Uuo7gWvZY5HQH4U6f5iU7cuv5YPM2HKJ"
        "gDZXZRoMR6t43aJWBRLjibkFqvHuWj8LTArMrmLSVq"
    };
}

// Secret two.
static system::config::base58check noise_authority()
{
    return { "1111111111111111111111111111111Hrt6FG" };
}

static system::ec_secret to_secret(const system::data_chunk& value)
{
    return system::data_slice{ value }.to_array<system::ec_secret_size>();
}

static system::config::base58check noise_authority_key()
{
    system::ec_xonly key{};
    noise::certificate::to_xonly(key, to_secret(noise_authority()));
    return { noise::certificate::encode_authority(key) };
}

static system::config::base58check noise_issued()
{
    system::ec_xonly key{};
    noise::certificate::data out{};
    noise::certificate::to_xonly(key, to_secret(noise_secret()));
    noise::certificate::sign(out, to_secret(noise_authority()), key, 0,
        max_uint32);
    return { system::to_chunk(out) };
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_authority_without_secret__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_authority = noise_authority();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_certificate_without_secret__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_certificate = noise_certificate();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_authority_key_without_secret__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_authority_key = noise_authority_key();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_secret_alone__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_secret_and_authority__issuing_context)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_authority = noise_authority();
    const auto ec = instance.initialize_context();
    BC_REQUIRE(!ec);
    BC_REQUIRE(instance.noise);
    BC_REQUIRE(instance.noise->valid());
    BC_REQUIRE(instance.noise->authority().has_value());
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_validity_days__issued_window)
{
    constexpr uint32_t now = 1800000000;
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_authority = noise_authority();
    instance.noise_validity_days = 2;
    noise::certificate::data out{};
    const auto ec = instance.initialize_context();
    const auto issued = instance.noise->issue(out, now);
    BC_REQUIRE(!ec);
    BC_REQUIRE(issued);
    BC_REQUIRE_EQUAL(noise::certificate::valid_from(out), now - 172800u);
    BC_REQUIRE_EQUAL(noise::certificate::not_valid_after(out), now + 172800u);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_secret_and_certificate__pre_issued_context)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_certificate = noise_certificate();
    const auto ec = instance.initialize_context();
    BC_REQUIRE(!ec);
    BC_REQUIRE(instance.noise);
    BC_REQUIRE(instance.noise->valid());
    BC_REQUIRE(!instance.noise->authority().has_value());
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_authority_and_certificate__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_authority = noise_authority();
    instance.noise_certificate = noise_certificate();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_short_certificate__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_certificate = system::data_chunk(73);
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_short_secret__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = system::data_chunk(31);
    instance.noise_authority = noise_authority();
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_invalid_secret__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = system::data_chunk(32);
    instance.noise_authority = noise_authority();
    const auto ec = instance.initialize_context();
    BC_REQUIRE(ec == network::error::invalid_configuration);
    BC_REQUIRE(!instance.noise);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_authority_key_issuing__success)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_authority = noise_authority();
    instance.noise_authority_key = noise_authority_key();
    const auto ec = instance.initialize_context();
    BC_REQUIRE(!ec);
    BC_REQUIRE(instance.noise);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_authority_key_certificate__success)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_certificate = noise_issued();
    instance.noise_authority_key = noise_authority_key();
    const auto ec = instance.initialize_context();
    BC_REQUIRE(!ec);
    BC_REQUIRE(instance.noise);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_other_authority_key__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_certificate = noise_issued();
    instance.noise_authority_key = noise_secret_key();
    const auto ec = instance.initialize_context();
    BC_REQUIRE(ec == network::error::invalid_configuration);
    BC_REQUIRE(!instance.noise);
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__initialize_context_malformed_authority_key__invalid_configuration)
{
    server::settings::stratum_v2_server instance{ "stratum_v2" };
    instance.noise_secret = noise_secret();
    instance.noise_authority = noise_authority();
    instance.noise_authority_key = system::data_chunk(34);
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__btcd_server__initialize_context_enable_utxo__no_timeouts)
{
    server::settings::btcd_server instance{ "btcd" };
    instance.enable_utxo = true;
    BC_REQUIRE(!instance.initialize_context());
    BC_REQUIRE_EQUAL(instance.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(instance.expiration_minutes, 0u);
}

BC_AUTO_TEST_CASE(server__bitcoind_server__initialize_context_minimum_below_implemented__invalid_configuration)
{
    server::settings::bitcoind_server instance{ "bitcoind" };
    instance.protocol_minimum = 299999;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__bitcoind_server__initialize_context_maximum_above_implemented__invalid_configuration)
{
    server::settings::bitcoind_server instance{ "bitcoind" };
    instance.protocol_maximum = 320001;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__bitcoind_server__initialize_context_minimum_above_maximum__invalid_configuration)
{
    server::settings::bitcoind_server instance{ "bitcoind" };
    instance.protocol_minimum = 320001;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__esplora_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.esplora;

    BC_REQUIRE_EQUAL(server.name, "esplora");
    BC_REQUIRE_EQUAL(server.maximum_history, 1'000'000u);
    BC_REQUIRE_EQUAL(server.protocol_version, 30000u);
}

BC_AUTO_TEST_CASE(server__esplora_server__initialize_context_defaults__success)
{
    server::settings::esplora_server instance{ "esplora" };
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__esplora_server__initialize_context_minimum__success)
{
    server::settings::esplora_server instance{ "esplora" };
    instance.protocol_version = 21000;
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__esplora_server__initialize_context_between_implemented__success)
{
    server::settings::esplora_server instance{ "esplora" };
    instance.protocol_version = 29999;
    BC_REQUIRE(!instance.initialize_context());
}

BC_AUTO_TEST_CASE(server__esplora_server__initialize_context_below_implemented__invalid_configuration)
{
    server::settings::esplora_server instance{ "esplora" };
    instance.protocol_version = 20999;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__esplora_server__initialize_context_above_implemented__invalid_configuration)
{
    server::settings::esplora_server instance{ "esplora" };
    instance.protocol_version = 30001;
    BC_REQUIRE(instance.initialize_context() == network::error::invalid_configuration);
}

BC_AUTO_TEST_CASE(server__electrum_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.electrum;

    // tcp_server
    BC_REQUIRE_EQUAL(server.name, "electrum");
    BC_REQUIRE(server.binds.empty());
    BC_REQUIRE_EQUAL(server.connections, 0u);
    BC_REQUIRE_EQUAL(server.inactivity_minutes, 10u);
    BC_REQUIRE_EQUAL(server.expiration_minutes, 60u);
    BC_REQUIRE(!server.enabled());
    BC_REQUIRE(server.inactivity() == minutes(10));
    BC_REQUIRE(server.expiration() == minutes(60));

    // tls_server
    BC_REQUIRE(!server.secure());
    BC_REQUIRE(server.safes.empty());
    BC_REQUIRE(server.cert_auth.empty());
    BC_REQUIRE(server.cert_path.empty());
    BC_REQUIRE(server.key_path.empty());
    BC_REQUIRE(server.key_pass.empty());

    // electrum_server
    BC_REQUIRE_EQUAL(server.maximum_headers, 10u * 2016u);
    BC_REQUIRE_EQUAL(server.maximum_history, 1'000'000u);
    BC_REQUIRE_EQUAL(server.maximum_subscriptions, 1'000'000u);
    BC_REQUIRE_EQUAL(server.protocol_minimum, version(1, 0, 0, 0));
    BC_REQUIRE_EQUAL(server.protocol_maximum, version(1, 7, 0, 0));
    BC_REQUIRE_EQUAL(server.server_name, BC_ELECTRUM_SERVER_NAME);
    BC_REQUIRE(server.donation_address.empty());
    BC_REQUIRE(server.banner_message.empty());
    BC_REQUIRE(server.self_binds.empty());
    BC_REQUIRE(server.self_safes.empty());
    BC_REQUIRE(server.more_binds.empty());
    BC_REQUIRE(server.more_safes.empty());
}

BC_AUTO_TEST_CASE(server__stratum_v1_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.stratum_v1;

    // stratum_v1_server
    BC_REQUIRE(server.payout_address.empty());
    BC_REQUIRE(server.coinbase_tag.empty());
    BC_REQUIRE_EQUAL(server.extranonce1_size, 4u);
    BC_REQUIRE_EQUAL(server.extranonce2_size, 4u);
    BC_REQUIRE_EQUAL(server.minimum_difficulty, 1.0);
    BC_REQUIRE_EQUAL(server.start_difficulty, 0.0);
    BC_REQUIRE_EQUAL(server.share_interval_seconds, 10u);
    BC_REQUIRE_EQUAL(server.authorize_timeout_seconds, 15u);

    // tcp_server
    BC_REQUIRE_EQUAL(server.name, "stratum_v1");
    BC_REQUIRE(server.binds.empty());
    BC_REQUIRE_EQUAL(server.connections, 0u);
    BC_REQUIRE_EQUAL(server.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(server.expiration_minutes, 0u);
    BC_REQUIRE(!server.enabled());
    BC_REQUIRE(server.inactivity() == minutes(0));
    BC_REQUIRE(server.expiration() == minutes(0));

    // tls_server
    BC_REQUIRE(!server.secure());
    BC_REQUIRE(server.safes.empty());
    BC_REQUIRE(server.cert_auth.empty());
    BC_REQUIRE(server.cert_path.empty());
    BC_REQUIRE(server.key_path.empty());
    BC_REQUIRE(server.key_pass.empty());
}

BC_AUTO_TEST_CASE(server__stratum_v2_server__defaults__expected)
{
    const server::settings::embedded_pages admin{};
    const server::settings::embedded_pages native{};
    const server::settings instance{ selection::none, native, admin };
    const auto& server = instance.stratum_v2;

    // stratum_v2_server
    BC_REQUIRE(server.payout_address.empty());
    BC_REQUIRE(server.coinbase_tag.empty());
    BC_REQUIRE_EQUAL(server.extranonce_prefix_size, 4u);
    BC_REQUIRE_EQUAL(server.extranonce_size, 8u);
    BC_REQUIRE_EQUAL(server.maximum_channels, 1024u);
    BC_REQUIRE_EQUAL(server.minimum_difficulty, 1.0);
    BC_REQUIRE_EQUAL(server.start_difficulty, 0.0);
    BC_REQUIRE_EQUAL(server.share_interval_seconds, 10u);
    BC_REQUIRE_EQUAL(server.authorize_timeout_seconds, 15u);
    BC_REQUIRE_EQUAL(server.noise_validity_days, 1u);
    BC_REQUIRE(!server.noise);

    // tcp_server
    BC_REQUIRE_EQUAL(server.name, "stratum_v2");
    BC_REQUIRE(server.binds.empty());
    BC_REQUIRE_EQUAL(server.connections, 0u);
    BC_REQUIRE_EQUAL(server.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(server.expiration_minutes, 0u);
    BC_REQUIRE(!server.enabled());
    BC_REQUIRE(server.inactivity() == minutes(0));
    BC_REQUIRE(server.expiration() == minutes(0));
}


// [wallet]

#define MAINNET_M "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi"
#define TESTNET_M "tprv8ZgxMBicQKsPeDgjzdC36fs6bMjGApWDNLR9erAXMs5skhMv36j9MV5ecvfavji5khqjWaWSFhN3YcCUUdiKH6isR4Pwy3U5y5egddBr16m"

BC_AUTO_TEST_CASE(wallet__defaults__mainnet__expected)
{
    const server::settings::wallet_settings instance{ selection::mainnet };
    BC_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::main::btc);
    BC_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::main::btc);
    BC_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::main::btc);
    BC_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::main::btc);
    BC_REQUIRE_EQUAL(instance.silent_prefix, prefix::sp::main::btc);
    BC_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::main::btc.prv);
    BC_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::main::btc.pub);
}

BC_AUTO_TEST_CASE(wallet__defaults__testnet__expected)
{
    const server::settings::wallet_settings instance{ selection::testnet3 };
    BC_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::test::btc);
    BC_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::test::btc);
    BC_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::test::btc);
    BC_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::test::btc);
    BC_REQUIRE_EQUAL(instance.silent_prefix, prefix::sp::test::btc);
    BC_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::test::btc.prv);
    BC_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::test::btc.pub);
}

BC_AUTO_TEST_CASE(wallet__defaults__signet__testnet_expected)
{
    const server::settings::wallet_settings instance{ selection::signet };
    BC_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::test::btc);
    BC_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::test::btc);
    BC_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::test::btc);
    BC_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::test::btc);
    BC_REQUIRE_EQUAL(instance.silent_prefix, prefix::sp::test::btc);
    BC_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::test::btc.prv);
    BC_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::test::btc.pub);
}

BC_AUTO_TEST_CASE(wallet__defaults__regtest__testnet_versions_with_regtest_witness)
{
    const server::settings::wallet_settings instance{ selection::regtest };
    BC_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::test::btc);
    BC_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::test::btc);
    BC_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::test::btc);
    BC_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::regtest::btc);
    BC_REQUIRE_EQUAL(instance.silent_prefix, prefix::sp::regtest::btc);
    BC_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::test::btc.prv);
}

BC_AUTO_TEST_CASE(wallet__to_context__mainnet__matches_predefined)
{
    const server::settings::wallet_settings instance{ selection::mainnet };
    BC_REQUIRE_EQUAL(instance.to_context().hd_prefixes(), ctx::btc::main.hd_prefixes());
    BC_REQUIRE_EQUAL(instance.to_context().versions(), ctx::btc::main.versions());
    BC_REQUIRE_EQUAL(instance.to_context().p2w, ctx::btc::main.p2w);
    BC_REQUIRE_EQUAL(instance.to_context().sp, ctx::btc::main.sp);
}

BC_AUTO_TEST_CASE(wallet__to_context__regtest__matches_predefined)
{
    const server::settings::wallet_settings instance{ selection::regtest };
    BC_REQUIRE_EQUAL(instance.to_context().hd_prefixes(), ctx::btc::regtest.hd_prefixes());
    BC_REQUIRE_EQUAL(instance.to_context().versions(), ctx::btc::regtest.versions());
    BC_REQUIRE_EQUAL(instance.to_context().p2w, ctx::btc::regtest.p2w);
    BC_REQUIRE_EQUAL(instance.to_context().sp, ctx::btc::regtest.sp);
}

BC_AUTO_TEST_CASE(wallet__to_context__testnet__parses_testnet_descriptor_only)
{
    const server::settings::wallet_settings instance{ selection::testnet3 };
    BC_REQUIRE(descriptor("pkh(" TESTNET_M "/1/*)", instance.to_context()));
    BC_REQUIRE(!descriptor("pkh(" MAINNET_M "/1/*)", instance.to_context()));
}

BC_AUTO_TEST_CASE(wallet__to_context__mainnet__parses_mainnet_descriptor_only)
{
    const server::settings::wallet_settings instance{ selection::mainnet };
    BC_REQUIRE(descriptor("pkh(" MAINNET_M "/1/*)", instance.to_context()));
    BC_REQUIRE(!descriptor("pkh(" TESTNET_M "/1/*)", instance.to_context()));
}

BC_AUTO_TEST_CASE(wallet__to_context__configured_prefixes__override_network)
{
    server::settings::wallet_settings instance{ selection::mainnet };
    instance.hd_private_prefix = prefix::hd::test::btc.prv;
    instance.hd_public_prefix = prefix::hd::test::btc.pub;
    BC_REQUIRE(descriptor("pkh(" TESTNET_M "/1/*)", instance.to_context()));
}

BC_AUTO_TEST_CASE(wallet__default_construct__mainnet__expected)
{
    const server::settings::wallet_settings instance{};
    BC_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::main::btc);
    BC_REQUIRE_EQUAL(instance.p2sh_prefix, prefix::p2sh::main::btc);
    BC_REQUIRE_EQUAL(instance.wif_prefix, prefix::wif::main::btc);
    BC_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::main::btc);
    BC_REQUIRE_EQUAL(instance.hd_private_prefix, prefix::hd::main::btc.prv);
    BC_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::main::btc.pub);
}

BC_AUTO_TEST_CASE(wallet__copy_construct__testnet__expected)
{
    const server::settings::wallet_settings original{ selection::testnet3 };
    const server::settings::wallet_settings instance{ original };
    BC_REQUIRE_EQUAL(instance.p2kh_prefix, prefix::p2kh::test::btc);
    BC_REQUIRE_EQUAL(instance.witness_prefix, prefix::p2w::test::btc);
    BC_REQUIRE_EQUAL(instance.hd_public_prefix, prefix::hd::test::btc.pub);
}

BC_AUTO_TEST_CASE(server__settings__initialize__native_safes_without_certificate__native_failure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    instance.native.safes.emplace_back("127.0.0.1:443");
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::tls_use_certificate);
    BC_REQUIRE_EQUAL(instance.failure, "native");
}

BC_AUTO_TEST_CASE(server__settings__initialize__default__no_failure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.failure.empty());
}

// [network]

BC_AUTO_TEST_CASE(server__settings__initialize_mainnet__service_ports)
{
    const server::settings::embedded_pages pages{};
    server::settings instance{ selection::mainnet, pages, pages };
    BC_REQUIRE(instance.network == selection::mainnet);
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.admin.bind_port, 8080u);
    BC_REQUIRE_EQUAL(instance.admin.safe_port, 8043u);
    BC_REQUIRE_EQUAL(instance.native.bind_port, 8180u);
    BC_REQUIRE_EQUAL(instance.native.safe_port, 8143u);
    BC_REQUIRE_EQUAL(instance.bitcoind.bind_port, 8280u);
    BC_REQUIRE_EQUAL(instance.bitcoind.safe_port, 8243u);
    BC_REQUIRE_EQUAL(instance.zmq.bind_port, 8380u);
    BC_REQUIRE_EQUAL(instance.zmq.safe_port, 8343u);
    BC_REQUIRE_EQUAL(instance.btcd.bind_port, 8480u);
    BC_REQUIRE_EQUAL(instance.btcd.safe_port, 8443u);
    BC_REQUIRE_EQUAL(instance.electrum.bind_port, 8580u);
    BC_REQUIRE_EQUAL(instance.electrum.safe_port, 8543u);
    BC_REQUIRE_EQUAL(instance.sparrow.bind_port, 8680u);
    BC_REQUIRE_EQUAL(instance.sparrow.safe_port, 8643u);
    BC_REQUIRE_EQUAL(instance.esplora.bind_port, 8780u);
    BC_REQUIRE_EQUAL(instance.esplora.safe_port, 8743u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.bind_port, 8880u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.safe_port, 8843u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.bind_port, 8980u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.safe_port, 8943u);
}

BC_AUTO_TEST_CASE(server__settings__mainnet__service_limits)
{
    const server::settings::embedded_pages pages{};
    const server::settings instance{ selection::mainnet, pages, pages };
    constexpr auto encoded = 2u * max_payload + 4u * kilobyte;
    BC_REQUIRE_EQUAL(instance.admin.maximum_backlog, 1024u * megabyte);
    BC_REQUIRE_EQUAL(instance.admin.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(instance.admin.expiration_minutes, 0u);
    BC_REQUIRE_EQUAL(instance.btcd.maximum_backlog, 20u * megabyte);
    BC_REQUIRE_EQUAL(instance.zmq.maximum_backlog, 32u * megabyte);
    BC_REQUIRE_EQUAL(instance.zmq.inactivity_minutes, 0u);
    BC_REQUIRE_EQUAL(instance.zmq.expiration_minutes, 0u);
    BC_REQUIRE_EQUAL(instance.bitcoind.maximum_request, encoded);
    BC_REQUIRE_EQUAL(instance.btcd.maximum_request, encoded);
    BC_REQUIRE_EQUAL(instance.electrum.maximum_request, encoded);
    BC_REQUIRE_EQUAL(instance.sparrow.maximum_request, encoded);
    BC_REQUIRE_EQUAL(instance.esplora.maximum_request, encoded);
    BC_REQUIRE(instance.bitcoind.error_status);
    BC_REQUIRE(!instance.btcd.error_status);
}

BC_AUTO_TEST_CASE(server__settings__initialize_testnet4__offset_ports_testnet_wallet)
{
    const server::settings::embedded_pages pages{};
    server::settings instance{ selection::testnet4, pages, pages };
    BC_REQUIRE(instance.network == selection::testnet4);
    BC_REQUIRE_EQUAL(instance.wallet.witness_prefix, "tb");
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.admin.bind_port, 48080u);
    BC_REQUIRE_EQUAL(instance.admin.safe_port, 48043u);
    BC_REQUIRE_EQUAL(instance.native.bind_port, 48180u);
    BC_REQUIRE_EQUAL(instance.native.safe_port, 48143u);
    BC_REQUIRE_EQUAL(instance.bitcoind.bind_port, 48280u);
    BC_REQUIRE_EQUAL(instance.bitcoind.safe_port, 48243u);
    BC_REQUIRE_EQUAL(instance.zmq.bind_port, 48380u);
    BC_REQUIRE_EQUAL(instance.zmq.safe_port, 48343u);
    BC_REQUIRE_EQUAL(instance.btcd.bind_port, 48480u);
    BC_REQUIRE_EQUAL(instance.btcd.safe_port, 48443u);
    BC_REQUIRE_EQUAL(instance.electrum.bind_port, 48580u);
    BC_REQUIRE_EQUAL(instance.electrum.safe_port, 48543u);
    BC_REQUIRE_EQUAL(instance.sparrow.bind_port, 48680u);
    BC_REQUIRE_EQUAL(instance.sparrow.safe_port, 48643u);
    BC_REQUIRE_EQUAL(instance.esplora.bind_port, 48780u);
    BC_REQUIRE_EQUAL(instance.esplora.safe_port, 48743u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.bind_port, 48880u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.safe_port, 48843u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.bind_port, 48980u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.safe_port, 48943u);
}

BC_AUTO_TEST_CASE(server__settings__initialize_testnet5__offset_ports_testnet_wallet)
{
    const server::settings::embedded_pages pages{};
    server::settings instance{ selection::testnet5, pages, pages };
    BC_REQUIRE(instance.network == selection::testnet5);
    BC_REQUIRE_EQUAL(instance.wallet.witness_prefix, "tb");
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.admin.bind_port, 58080u);
    BC_REQUIRE_EQUAL(instance.admin.safe_port, 58043u);
    BC_REQUIRE_EQUAL(instance.native.bind_port, 58180u);
    BC_REQUIRE_EQUAL(instance.native.safe_port, 58143u);
    BC_REQUIRE_EQUAL(instance.bitcoind.bind_port, 58280u);
    BC_REQUIRE_EQUAL(instance.bitcoind.safe_port, 58243u);
    BC_REQUIRE_EQUAL(instance.zmq.bind_port, 58380u);
    BC_REQUIRE_EQUAL(instance.zmq.safe_port, 58343u);
    BC_REQUIRE_EQUAL(instance.btcd.bind_port, 58480u);
    BC_REQUIRE_EQUAL(instance.btcd.safe_port, 58443u);
    BC_REQUIRE_EQUAL(instance.electrum.bind_port, 58580u);
    BC_REQUIRE_EQUAL(instance.electrum.safe_port, 58543u);
    BC_REQUIRE_EQUAL(instance.sparrow.bind_port, 58680u);
    BC_REQUIRE_EQUAL(instance.sparrow.safe_port, 58643u);
    BC_REQUIRE_EQUAL(instance.esplora.bind_port, 58780u);
    BC_REQUIRE_EQUAL(instance.esplora.safe_port, 58743u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.bind_port, 58880u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.safe_port, 58843u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.bind_port, 58980u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.safe_port, 58943u);
}

BC_AUTO_TEST_CASE(server__settings__initialize_regtest_network__offset_ports)
{
    const server::settings::embedded_pages pages{};
    server::settings instance{ selection::mainnet, pages, pages };
    instance.network = selection::regtest;
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.admin.bind_port, 28080u);
    BC_REQUIRE_EQUAL(instance.admin.safe_port, 28043u);
    BC_REQUIRE_EQUAL(instance.native.bind_port, 28180u);
    BC_REQUIRE_EQUAL(instance.native.safe_port, 28143u);
    BC_REQUIRE_EQUAL(instance.bitcoind.bind_port, 28280u);
    BC_REQUIRE_EQUAL(instance.bitcoind.safe_port, 28243u);
    BC_REQUIRE_EQUAL(instance.zmq.bind_port, 28380u);
    BC_REQUIRE_EQUAL(instance.zmq.safe_port, 28343u);
    BC_REQUIRE_EQUAL(instance.btcd.bind_port, 28480u);
    BC_REQUIRE_EQUAL(instance.btcd.safe_port, 28443u);
    BC_REQUIRE_EQUAL(instance.electrum.bind_port, 28580u);
    BC_REQUIRE_EQUAL(instance.electrum.safe_port, 28543u);
    BC_REQUIRE_EQUAL(instance.sparrow.bind_port, 28680u);
    BC_REQUIRE_EQUAL(instance.sparrow.safe_port, 28643u);
    BC_REQUIRE_EQUAL(instance.esplora.bind_port, 28780u);
    BC_REQUIRE_EQUAL(instance.esplora.safe_port, 28743u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.bind_port, 28880u);
    BC_REQUIRE_EQUAL(instance.stratum_v1.safe_port, 28843u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.bind_port, 28980u);
    BC_REQUIRE_EQUAL(instance.stratum_v2.safe_port, 28943u);
}

BC_AUTO_TEST_CASE(server__settings__initialize_unported_bind__default_port)
{
    const server::settings::embedded_pages pages{};
    server::settings instance{ selection::testnet4, pages, pages };
    instance.electrum.binds.emplace_back(system::ip::address{}, 0_u16);
    instance.electrum.binds.emplace_back(system::ip::address{}, 42_u16);
    BC_REQUIRE(!instance.initialize());
    BC_REQUIRE_EQUAL(instance.electrum.binds.front().port(), 48580u);
    BC_REQUIRE_EQUAL(instance.electrum.binds.back().port(), 42u);
}

BC_AUTO_TEST_SUITE_END()

// [tls]

struct tls_setup_fixture
  : test::directory_setup_fixture
{
    tls_setup_fixture()
    {
        std::filesystem::create_directories(clients);
        write_identity(certificate, private_key);
        write_identity(clients / "client.pem", directory / "client.key");
    }

    static void write_text(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(text.data(),
            system::possible_narrow_sign_cast<std::streamsize>(text.size()));
    }

    static void write_identity(const std::filesystem::path& certificate_path,
        const std::filesystem::path& key_path)
    {
        const auto key = system::base16_array("c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721");
        system::x509::subject subject{};
        subject.common_name = "localhost";
        subject.dns_names = { "localhost" };
        subject.not_before = 1735689600;
        subject.not_after = 2524608000;

        system::data_chunk der{};
        system::x509::build_self_signed(der, key, subject);
        write_text(certificate_path, system::x509::encode_certificate(der));
        write_text(key_path, system::x509::encode_private_key(key));
    }

    void secure(network::settings::tls_server& service) const
    {
        service.safes.emplace_back("127.0.0.1:443");
        service.cert_path = certificate;
        service.key_path = private_key;
    }

    const std::filesystem::path directory{ TEST_DIRECTORY };
    const std::filesystem::path certificate{ directory / "server.pem" };
    const std::filesystem::path private_key{ directory / "server.key" };
    const std::filesystem::path clients{ directory / "clients" };
};

BC_FIXTURE_TEST_SUITE(settings_tls_tests, tls_setup_fixture)

using namespace bc::system::chain;

BC_AUTO_TEST_CASE(server__settings__initialize__admin_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.admin);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.admin.secure());
    BC_REQUIRE(!instance.admin.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__admin_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.admin);
    instance.admin.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.admin.secure());
    BC_REQUIRE(instance.admin.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__native_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.native);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.native.secure());
    BC_REQUIRE(!instance.native.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__native_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.native);
    instance.native.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.native.secure());
    BC_REQUIRE(instance.native.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__bitcoind_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.bitcoind);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.bitcoind.secure());
    BC_REQUIRE(!instance.bitcoind.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__bitcoind_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.bitcoind);
    instance.bitcoind.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.bitcoind.secure());
    BC_REQUIRE(instance.bitcoind.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__btcd_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.btcd);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.btcd.secure());
    BC_REQUIRE(!instance.btcd.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__btcd_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.btcd);
    instance.btcd.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.btcd.secure());
    BC_REQUIRE(instance.btcd.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__electrum_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.electrum);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.electrum.secure());
    BC_REQUIRE(!instance.electrum.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__electrum_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.electrum);
    instance.electrum.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.electrum.secure());
    BC_REQUIRE(instance.electrum.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__sparrow_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.sparrow);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.sparrow.secure());
    BC_REQUIRE(!instance.sparrow.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__sparrow_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.sparrow);
    instance.sparrow.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.sparrow.secure());
    BC_REQUIRE(instance.sparrow.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__esplora_safes_with_certificate__secure)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.esplora);
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.esplora.secure());
    BC_REQUIRE(!instance.esplora.authenticate());
}

BC_AUTO_TEST_CASE(server__settings__initialize__esplora_certificate_authority__authenticate)
{
    const auto undefined = server::settings::embedded_pages{};
    server::settings instance{ selection::none, undefined, undefined };
    secure(instance.esplora);
    instance.esplora.cert_auth = clients;
    BC_REQUIRE_EQUAL(instance.initialize(), network::error::success);
    BC_REQUIRE(instance.esplora.secure());
    BC_REQUIRE(instance.esplora.authenticate());
}

BC_AUTO_TEST_SUITE_END()
