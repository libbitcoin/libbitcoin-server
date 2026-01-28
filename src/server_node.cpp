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
#include <bitcoin/server/server_node.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/sessions/sessions.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace database;
using namespace network;
using namespace node;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// net::strand() is safe to call from constructor (non-virtual).
server_node::server_node(query& query, const configuration& configuration,
    const logger& log) NOEXCEPT
  : full_node(query, configuration, log),
    config_(configuration)
{
}

server_node::~server_node() NOEXCEPT
{
}

// Properties.
// ----------------------------------------------------------------------------

const configuration& server_node::server_config() const NOEXCEPT
{
    return config_;
}

const settings& server_node::server_settings() const NOEXCEPT
{
    return config_.server;
}

// Sequences.
// ----------------------------------------------------------------------------

void server_node::run(result_handler&& handler) NOEXCEPT
{
    // Base (net) invokes do_run().
    full_node::run(std::move(handler));
}

void server_node::do_run(const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Start services after node is running.
    full_node::do_run(std::bind(&server_node::start_web, this, _1, handler));
}

void server_node::start_web(const code& ec,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        handler(ec);
        return;
    }

    attach_web_session()->start(
        std::bind(&server_node::start_native, this, _1, handler));
}

void server_node::start_native(const code& ec,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        handler(ec);
        return;
    }

    attach_native_session()->start(
        std::bind(&server_node::start_bitcoind, this, _1, handler));
}

void server_node::start_bitcoind(const code& ec,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        handler(ec);
        return;
    }

    attach_bitcoind_session()->start(
        std::bind(&server_node::start_electrum, this, _1, handler));
}

void server_node::start_electrum(const code& ec,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        handler(ec);
        return;
    }

    attach_electrum_session()->start(
        std::bind(&server_node::start_stratum_v1, this, _1, handler));
}

void server_node::start_stratum_v1(const code& ec,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        handler(ec);
        return;
    }

    attach_stratum_v1_session()->start(
        std::bind(&server_node::start_stratum_v2, this, _1, handler));
}

void server_node::start_stratum_v2(const code& ec,
    const result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (ec)
    {
        handler(ec);
        return;
    }

    attach_stratum_v2_session()->start(move_copy(handler));
}

// Session attachments.
// ----------------------------------------------------------------------------

session_web::ptr server_node::attach_web_session() NOEXCEPT
{
    return net::attach<session_web>(*this, config_,
        config_.server.web);
}

session_native::ptr server_node::attach_native_session() NOEXCEPT
{
    return net::attach<session_native>(*this, config_,
        config_.server.native);
}

session_bitcoind::ptr server_node::attach_bitcoind_session() NOEXCEPT
{
    return net::attach<session_bitcoind>(*this, config_,
        config_.server.bitcoind);
}

session_electrum::ptr server_node::attach_electrum_session() NOEXCEPT
{
    return net::attach<session_electrum>(*this, config_,
        config_.server.electrum);
}

session_stratum_v1::ptr server_node::attach_stratum_v1_session() NOEXCEPT
{
    return net::attach<session_stratum_v1>(*this, config_,
        config_.server.stratum_v1);
}

session_stratum_v2::ptr server_node::attach_stratum_v2_session() NOEXCEPT
{
    return net::attach<session_stratum_v2>(*this, config_,
        config_.server.stratum_v2);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
