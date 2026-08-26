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
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

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
    SUBSCRIBE_BITCOIND(handle_get_openrpc_info, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_rpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_logging, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_uptime, _1, _2);
    SUBSCRIBE_BITCOIND(handle_rpc_discover, _1, _2, _3);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Control methods.
// ----------------------------------------------------------------------------

// One usage line synthesized from the interface metadata (not bitcoind's
// narrative help text). Optional parameters are parenthesized.
template <typename Method>
static void append_usage(std::string& out, const Method& entry,
    const std::string& command) NOEXCEPT
{
    if (!entry.implemented() || entry.name != command)
        return;

    out = command;
    const auto& names = entry.parameter_names();
    [&]<size_t... Index>(std::index_sequence<Index...>) NOEXCEPT
    {
        ((out += is_optional<std::tuple_element_t<Index,
            typename Method::args_native>> ?
                " ( " + std::string{ names.at(Index) } + " )" :
                " " + std::string{ names.at(Index) }), ...);
    }(std::make_index_sequence<Method::size>{});
}

template <typename Methods>
static void find_usage(std::string& out, const std::string& command) NOEXCEPT
{
    std::apply([&](const auto&... entries) NOEXCEPT
    {
        (append_usage(out, entries, command), ...);
    }, Methods::methods);
}

bool protocol_bitcoind_control::handle_help(const code& ec, rpc_interface::help,
    const std::string& command) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (command.empty())
    {
        auto names = help_names();
        const auto size = two * names.size();
        send_result(std::move(names), size);
        return true;
    }

    using namespace interface;
    std::string usage{};
    find_usage<bitcoind_blockchain_methods>(usage, command);
    find_usage<bitcoind_control_methods>(usage, command);
    find_usage<bitcoind_mining_methods>(usage, command);
    find_usage<bitcoind_network_methods>(usage, command);
    find_usage<bitcoind_notifications_methods>(usage, command);
    find_usage<bitcoind_test_methods>(usage, command);
    find_usage<bitcoind_transaction_methods>(usage, command);
    find_usage<bitcoind_utility_methods>(usage, command);
    find_usage<bitcoind_wallet_methods>(usage, command);

    // bitcoind reports an unknown command in the result text.
    if (usage.empty())
        usage = "help: unknown command: " + command;

    send_result(std::move(usage), 128);
    return true;
}

bool protocol_bitcoind_control::handle_stop(const code& ec,
    rpc_interface::stop) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
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
        send_error(error::bitcoind::invalid_parameter);
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

template <typename Method>
static void append_method(array_t& out, const Method& entry) NOEXCEPT
{
    if (!entry.implemented())
        return;

    array_t parameters{};
    const auto& names = entry.parameter_names();
    [&]<size_t... Index>(std::index_sequence<Index...>) NOEXCEPT
    {
        (parameters.emplace_back(object_t
        {
            { "name", std::string{ names.at(Index) } },
            { "required", !is_optional<std::tuple_element_t<Index,
                typename Method::args_native>> }
        }), ...);
    }(std::make_index_sequence<Method::size>{});

    out.emplace_back(object_t
    {
        { "name", std::string{ entry.name } },
        { "params", std::move(parameters) }
    });
}

template <typename Methods>
static void append_methods(array_t& out) NOEXCEPT
{
    std::apply([&](const auto&... entries) NOEXCEPT
    {
        (append_method(out, entries), ...);
    }, Methods::methods);
}

// There are no hidden methods (unimplemented rows are refusals, not hidden).
void protocol_bitcoind_control::send_openrpc() NOEXCEPT
{
    using namespace interface;
    array_t methods{};
    append_methods<bitcoind_blockchain_methods>(methods);
    append_methods<bitcoind_control_methods>(methods);
    append_methods<bitcoind_mining_methods>(methods);
    append_methods<bitcoind_network_methods>(methods);
    append_methods<bitcoind_notifications_methods>(methods);
    append_methods<bitcoind_test_methods>(methods);
    append_methods<bitcoind_transaction_methods>(methods);
    append_methods<bitcoind_utility_methods>(methods);
    append_methods<bitcoind_wallet_methods>(methods);

    const auto& settings = server_settings().bitcoind;
    const auto size = 64 * methods.size();
    send_result(object_t
    {
        { "openrpc", std::string{ "1.2.6" } },
        { "info", object_t
        {
            { "title", settings.subversion },
            { "version", settings.version.to_string() }
        } },
        { "methods", std::move(methods) }
    }, size);
}

bool protocol_bitcoind_control::handle_get_openrpc_info(const code& ec,
    rpc_interface::get_openrpc_info, bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_openrpc();
    return true;
}

// bitcoind's discovery alias for the openrpc document.
bool protocol_bitcoind_control::handle_rpc_discover(const code& ec,
    rpc_interface::rpc_discover, bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_openrpc();
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
        send_error(error::bitcoind::invalid_parameter);
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
