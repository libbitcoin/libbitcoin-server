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
#include "native_setup_fixture.hpp"

namespace http = network::http;
namespace ws = network::ws;

BC_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// configuration
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__configuration__json__expected)
{
    const auto response = get_json("/v1/configuration?format=json");
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BC_REQUIRE_EQUAL(object.size(), 8u);
    BC_REQUIRE_EQUAL(object.at("network").as_string(), "mainnet");
    BC_REQUIRE(object.at("address").as_bool());
    BC_REQUIRE(object.at("filter").as_bool());
    BC_REQUIRE(!object.at("turbo").as_bool());
    BC_REQUIRE(object.at("witness").as_bool());
    BC_REQUIRE(object.at("retarget").as_bool());
    BC_REQUIRE(object.at("difficult").as_bool());
    BC_REQUIRE_EQUAL(object.at("pruned").as_int64(), 0);
}

BC_AUTO_TEST_CASE(native__configuration__text__not_acceptable)
{
    BC_REQUIRE_EQUAL(get_status("/v1/configuration?format=text"), http::status::not_acceptable);
}

BC_AUTO_TEST_CASE(native__configuration__data__not_acceptable)
{
    BC_REQUIRE_EQUAL(get_status("/v1/configuration?format=data"), http::status::not_acceptable);
}

BC_AUTO_TEST_CASE(native__ws_configuration__default__json)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/configuration");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE(response.as_object().at("witness").as_bool());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(native_limited_tests, native_limited_setup_fixture)

BC_AUTO_TEST_CASE(native__configuration__limited_blocks_unreached_milestone__pruned_zero)
{
    const auto response = get_json("/v1/configuration?format=json");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("pruned").as_int64(), 0);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(server_node_tests, server_node_setup_fixture)

static const std::string unavailable{ "192.0.2.1:65009" };

BC_AUTO_TEST_CASE(server_node__server_config__always__configuration)
{
    BC_REQUIRE(&server_.server_config() == &config_);
}

BC_AUTO_TEST_CASE(server_node__server_settings__always__server_settings)
{
    BC_REQUIRE(&server_.server_settings() == &config_.server);
}

BC_AUTO_TEST_CASE(server_node__run__default__success)
{
    BC_REQUIRE(!run([](server::configuration&) {}));
}

BC_AUTO_TEST_CASE(server_node__run__inbound_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.network.inbound.binds = { { unavailable } };
        config.network.inbound.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__admin_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.admin.path = "unused";
        config.server.admin.binds = { { unavailable } };
        config.server.admin.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__native_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.native.path = "unused";
        config.server.native.binds = { { unavailable } };
        config.server.native.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__bitcoind_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.bitcoind.binds = { { unavailable } };
        config.server.bitcoind.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__btcd_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.btcd.binds = { { unavailable } };
        config.server.btcd.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__electrum_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.electrum.binds = { { unavailable } };
        config.server.electrum.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__sparrow_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.sparrow.binds = { { unavailable } };
        config.server.sparrow.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__esplora_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.esplora.binds = { { unavailable } };
        config.server.esplora.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__stratum_v1_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.stratum_v1.binds = { { unavailable } };
        config.server.stratum_v1.connections = 1;
    }));
}

BC_AUTO_TEST_CASE(server_node__run__stratum_v2_unavailable__failure)
{
    BC_REQUIRE(run([](server::configuration& config)
    {
        config.server.stratum_v2.binds = { { unavailable } };
        config.server.stratum_v2.connections = 1;
    }));
}

BC_AUTO_TEST_SUITE_END()
