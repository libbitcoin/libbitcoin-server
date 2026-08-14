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
#include <bitcoin/server/protocols/protocol_bitcoind_control.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

namespace server {

#define CLASS protocol_bitcoind_control
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_control::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_help, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_stop, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_memory_info, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_openrpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_rpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_logging, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_uptime, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Control methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_control::handle_help(const code& ec, rpc_interface::help,
    const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    auto names = help_names();
    const auto size = two * names.size();
    send_result(std::move(names), size);
    return true;
}

bool protocol_bitcoind_control::handle_stop(const code& ec,
    rpc_interface::stop) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

// libbitcoin has no locked memory manager (bitcoind locks pages holding key
// material), so nothing is locked and the arena is empty, not unknown.
bool protocol_bitcoind_control::handle_get_memory_info(const code& ec,
    rpc_interface::get_memory_info, const std::string& mode) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // mallocinfo is a glibc-specific malloc dump.
    if (mode != "stats")
    {
        send_error(error::invalid_argument);
        return true;
    }

    object_t locked
    {
        { "used", zero },
        { "free", zero },
        { "total", zero },
        { "locked", zero },
        { "chunks_used", zero },
        { "chunks_free", zero }
    };

    send_result(object_t
    {
        { "locked", std::move(locked) }
    }, 192);
    return true;
}

bool protocol_bitcoind_control::handle_get_openrpc_info(const code& ec,
    rpc_interface::get_openrpc_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

// Dispatch is synchronous on the channel strand, so there is no set of
// in-flight commands to report (bitcoind lists its own call here).
bool protocol_bitcoind_control::handle_get_rpc_info(const code& ec,
    rpc_interface::get_rpc_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(object_t
    {
        { "active_commands", array_t{} },
        { "logpath", server_config().log.log_file1().string() }
    }, 128);
    return true;
}

// libbitcoin logs by level, where bitcoind logs by category. A level is
// reported active when compiled in and enabled by configuration. Levels are
// configured, so the set cannot be changed at run time.
bool protocol_bitcoind_control::handle_logging(const code& ec,
    rpc_interface::logging, const array_t& include,
    const array_t& exclude) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!include.empty() || !exclude.empty())
    {
        send_error(error::invalid_argument);
        return true;
    }

    using namespace network::levels;
    const auto& out = server_config().log;
    send_result(object_t
    {
        { "application", application_defined && out.application },
        { "news", news_defined && out.news },
        { "session", session_defined && out.session },
        { "protocol", protocol_defined && out.protocol },
        { "proxy", proxy_defined && out.proxy },
        { "remote", remote_defined && out.remote },
        { "fault", fault_defined && out.fault },
        { "quitting", quitting_defined && out.quitting },
        { "objects", objects_defined && out.objects },
        { "verbose", verbose_defined && out.verbose }
    }, 256);
    return true;
}

bool protocol_bitcoind_control::handle_uptime(const code& ec,
    rpc_interface::uptime) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(floored_subtract(to_unsigned(zulu_time()),
        to_unsigned(start_time())), 20);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
