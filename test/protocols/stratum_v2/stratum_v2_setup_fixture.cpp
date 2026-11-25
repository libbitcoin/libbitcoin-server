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
#include "stratum_v2_setup_fixture.hpp"

using namespace system;
using namespace network::messages::sv2;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

stratum_v2_setup_fixture::stratum_v2_setup_fixture(const initializer& setup,
    const configurator& configure)
  : rpc_setup_fixture(setup, [configure](configuration& config) NOEXCEPT
        {
            auto& stratum = config.server.stratum_v2;
            stratum.binds = { { STRATUM_V2_ENDPOINT } };
            stratum.connections = 1;
            stratum.inactivity_minutes = 1;
            config.node.currency_window_minutes = 0;

            if (configure)
                configure(config);
        }, true, true)
{
    client_.connect(binding());
}

stratum_v2_setup_fixture::~stratum_v2_setup_fixture()
{
    client_.close();
}

BC_POP_WARNING()

network::code stratum_v2_setup_fixture::upgrade()
{
    return client_.upgrade();
}

network::code stratum_v2_setup_fixture::upgrade(const std::string& username,
    const std::string& password)
{
    return client_.upgrade(username, password);
}

network::http::status stratum_v2_setup_fixture::get_status()
{
    return client_.get("/").result();
}

void stratum_v2_setup_fixture::send(const data_chunk& frame)
{
    client_.write_binary(frame);
}

data_chunk stratum_v2_setup_fixture::receive()
{
    return client_.read_binary();
}

data_chunk stratum_v2_setup_fixture::setup_frame(uint16_t minimum,
    uint16_t maximum, uint32_t flags, uint8_t protocol)
{
    return to_frame(setup_connection
    {
        protocol, minimum, maximum, flags, "127.0.0.1", 8980_u16, "vendor",
        "hardware", "firmware", "device"
    });
}

setup_connection_error::cptr stratum_v2_setup_fixture::get_error(
    const data_chunk& frame)
{
    send(frame);
    const auto reply = receive();
    const auto head = heading::deserialize(reply);
    if (!head || head->message_type != setup_connection_error::identifier)
        return {};

    const auto start = std::next(reply.begin(), heading::size());
    return setup_connection_error::deserialize(data_chunk{ start, reply.end() });
}
