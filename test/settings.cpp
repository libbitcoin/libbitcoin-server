/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
    BOOST_REQUIRE(instance.certificate_authority.empty());
    BOOST_REQUIRE(instance.certificate_path.empty());
    BOOST_REQUIRE(instance.key_path.empty());
    BOOST_REQUIRE(instance.key_password.empty());

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

BOOST_AUTO_TEST_CASE(server__web_server__defaults__expected)
{
    const server::settings::embedded_pages web{};
    const server::settings::embedded_pages explorer{};
    const server::settings instance{ selection::none, explorer, web };
    const auto& server = instance.web;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "web");
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
    BOOST_REQUIRE(server.certificate_authority.empty());
    BOOST_REQUIRE(server.certificate_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_password.empty());

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

BOOST_AUTO_TEST_CASE(server__explore_server__defaults__expected)
{
    const server::settings::embedded_pages web{};
    const server::settings::embedded_pages explorer{};
    const server::settings instance{ selection::none, explorer, web };
    const auto& server = instance.explore;

    // tcp_server
    BOOST_REQUIRE_EQUAL(server.name, "explore");
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
    BOOST_REQUIRE(server.certificate_authority.empty());
    BOOST_REQUIRE(server.certificate_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_password.empty());

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
    const server::settings::embedded_pages web{};
    const server::settings::embedded_pages explorer{};
    const server::settings instance{ selection::none, explorer, web };
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
    BOOST_REQUIRE(server.certificate_authority.empty());
    BOOST_REQUIRE(server.certificate_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_password.empty());

    // http_server
    BOOST_REQUIRE_EQUAL(server.server, BC_HTTP_SERVER_NAME);
    BOOST_REQUIRE(server.hosts.empty());
    BOOST_REQUIRE(server.host_names().empty());
}

BOOST_AUTO_TEST_CASE(server__electrum_server__defaults__expected)
{
    const server::settings::embedded_pages web{};
    const server::settings::embedded_pages explorer{};
    const server::settings instance{ selection::none, explorer, web };
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
    BOOST_REQUIRE(server.certificate_authority.empty());
    BOOST_REQUIRE(server.certificate_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_password.empty());
}

BOOST_AUTO_TEST_CASE(server__stratum_v1_server__defaults__expected)
{
    const server::settings::embedded_pages web{};
    const server::settings::embedded_pages explorer{};
    const server::settings instance{ selection::none, explorer, web };
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
    BOOST_REQUIRE(server.certificate_authority.empty());
    BOOST_REQUIRE(server.certificate_path.empty());
    BOOST_REQUIRE(server.key_path.empty());
    BOOST_REQUIRE(server.key_password.empty());
}

BOOST_AUTO_TEST_CASE(server__stratum_v2_server__defaults__expected)
{
    const server::settings::embedded_pages web{};
    const server::settings::embedded_pages explorer{};
    const server::settings instance{ selection::none, explorer, web };
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

BOOST_AUTO_TEST_SUITE_END()
