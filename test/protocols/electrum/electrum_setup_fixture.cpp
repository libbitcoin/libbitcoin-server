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
#include "../../mocks/blocks.hpp"
#include "electrum_setup_fixture.hpp"
#include <future>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

electrum_setup_fixture::electrum_setup_fixture(const initializer& setup,
    bool address_index, const configurator& configure, service which)
  : rpc_setup_fixture(setup,
        [which, configure](configuration& config) NOEXCEPT
        {
            // Only the configured service binds.
            const auto sparrow = (which == service::sparrow);
            auto& electrum = sparrow ?
                static_cast<server::settings::electrum_server&>(
                    config.server.sparrow) :
                config.server.electrum;

            electrum.binds =
            {
                { sparrow ? SPARROW_ENDPOINT : ELECTRUM_ENDPOINT }
            };

            electrum.server_name = "server_name";
            electrum.banner_message = "banner_message";
            electrum.donation_address = "donation_address";
            electrum.maximum_subscriptions = 2;
            electrum.maximum_history = 5;
            electrum.maximum_headers = 5;
            electrum.connections = 1;
            electrum.inactivity_minutes = 1;
            config.node.fee_estimate_horizon = 8;

            if (configure)
                configure(config);
        }, address_index, true),
    which_{ which }
{
    client_.connect(options().binds.back().to_endpoint());
}

electrum_setup_fixture::~electrum_setup_fixture()
{
    client_.close();
}

BC_POP_WARNING()

int64_t electrum_setup_fixture::get_error(const std::string& request)
{
    try
    {
        return get(request).at("error").as_object().at("code").as_int64();
    }
    catch (const boost::system::system_error&)
    {
        return -1;
    }
}

boost::json::value electrum_setup_fixture::get(const std::string& request)
{
    return client_.send(request);
}

boost::json::value electrum_setup_fixture::receive()
{
    return client_.receive();
}

const server::settings::electrum_server&
electrum_setup_fixture::options() const
{
    return which_ == service::sparrow ?
        static_cast<const server::settings::electrum_server&>(
            config_.server.sparrow) :
        config_.server.electrum;
}

bool electrum_setup_fixture::verify(const boost::json::value& response,
    electrum::version version, network::rpc::code_t id) const
{
    try
    {
        if (response.at("id").as_int64() != id)
            return false;

        // The 1.0 result is the server software string alone.
        if (version == electrum::version::v1_0)
            return response.at("result").is_string() &&
                (response.at("result").as_string() == options().server_name);

        // Assumes server always accepts proposed version.
        const auto& result = response.at("result").as_array();
        return (result.size() == two) &&
            (result.at(0).is_string() && result.at(1).is_string()) &&
            (result.at(0).as_string() == options().server_name) &&
            (result.at(1).as_string() == electrum::version_to_string(version));

    }
    catch (...)
    {
        return false;
    }
}

static std::string version_request(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return (boost_format
    (
        R"({"id":%1%,"method":"server.version","params":["%2%","%3%"]})"
    ) % id % name % electrum::version_to_string(version)).str();
}

bool electrum_setup_fixture::handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return verify(get(version_request(version, name, id) + "\n"), version, id);
}

// http POST.
// ----------------------------------------------------------------------------

boost::json::value electrum_setup_fixture::post(const std::string& request)
{
    return client_.post(request);
}

bool electrum_setup_fixture::post_handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return verify(post(version_request(version, name, id)), version, id);
}

// websocket.
// ----------------------------------------------------------------------------

network::boost_code electrum_setup_fixture::ws_upgrade()
{
    return client_.upgrade();
}

boost::json::value electrum_setup_fixture::ws_receive()
{
    return client_.read_frame();
}

boost::json::value electrum_setup_fixture::ws_get(const std::string& request)
{
    return client_.frame(request);
}

bool electrum_setup_fixture::ws_handshake(electrum::version version,
    const std::string& name, network::rpc::code_t id)
{
    return verify(ws_get(version_request(version, name, id)), version, id);
}
