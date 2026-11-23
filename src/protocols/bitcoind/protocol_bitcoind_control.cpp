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
using namespace system::json;

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

std::string protocol_bitcoind_control::usage(
    const std::string& command) const NOEXCEPT
{
    using namespace interface;
    const auto version = interface_version();
    std::string out{};
    find_usage<bitcoind_blockchain_methods>(out, command, version);
    find_usage<bitcoind_control_methods>(out, command, version);
    find_usage<bitcoind_mining_methods>(out, command, version);
    find_usage<bitcoind_network_methods>(out, command, version);
    find_usage<bitcoind_notifications_methods>(out, command, version);
    find_usage<bitcoind_test_methods>(out, command, version);
    find_usage<bitcoind_transaction_methods>(out, command, version);
    find_usage<bitcoind_utility_methods>(out, command, version);
    find_usage<bitcoind_wallet_methods>(out, command, version);
    return out;
}

void protocol_bitcoind_control::send_help_list() NOEXCEPT
{
    send_result(help_names());
}

// bitcoind reports an unknown command in the result text.
void protocol_bitcoind_control::send_help_unknown(
    const std::string& command) NOEXCEPT
{
    send_result("help: unknown command: " + command);
}

void protocol_bitcoind_control::handle_help(const code& ec, rpc_interface::help,
    const std::string& command) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (command.empty())
    {
        send_help_list();
        return;
    }

    auto line = usage(command);
    if (line.empty())
        send_help_unknown(command);
    else
        send_result(std::move(line));
}

void protocol_bitcoind_control::handle_stop(const code& ec,
    rpc_interface::stop) NOEXCEPT
{
    if (stopped(ec)) return;
    send_error(error::bitcoind::method_not_found);
}

void protocol_bitcoind_control::handle_get_memory_info(const code& ec,
    rpc_interface::get_memory_info, const std::string& mode) NOEXCEPT
{
    if (stopped(ec))
        return;

    // mallocinfo is a glibc-specific malloc dump.
    if (mode != "stats")
    {
        send_error(error::bitcoind::invalid_parameter);
        return;
    }

    // Nothing is locked and the arena is empty, not unknown.
    object_t locked
    {
        { "used", to64(zero) },
        { "free", to64(zero) },
        { "total", to64(zero) },
        { "locked", to64(zero) },
        { "chunks_used", to64(zero) },
        { "chunks_free", to64(zero) }
    };

    send_result(object_t
    {
        { "locked", std::move(locked) }
    });
}

template <typename Method>
static void append_method(array_t& out, const Method& entry,
    interface::bitcoind::version version) NOEXCEPT
{
    if (!entry.implemented() ||
        version < interface::bitcoind::introduced(entry.name))
        return;

    array_t parameters{};
    const auto& names = entry.parameter_names();
    const auto append = [&](const auto& name, bool required) NOEXCEPT
    {
        parameters.emplace_back(object_t
        {
            { "name", std::string{ primary_name(name) } },
            { "required", required }
        });
    };

    [&]<size_t... Index>(std::index_sequence<Index...>) NOEXCEPT
    {
        (append(names.at(Index), !is_defaulted<std::tuple_element_t<Index,
            typename Method::args_native>>), ...);
    }(std::make_index_sequence<Method::size>{});

    out.emplace_back(object_t
    {
        { "name", std::string{ entry.name } },
        { "params", std::move(parameters) }
    });
}

template <typename Methods>
static void append_methods(array_t& out,
    interface::bitcoind::version version) NOEXCEPT
{
    std::apply([&](const auto&... entries) NOEXCEPT
    {
        (append_method(out, entries, version), ...);
    }, Methods::methods);
}

// There are no hidden methods (unimplemented rows are refusals, not hidden).
void protocol_bitcoind_control::send_openrpc() NOEXCEPT
{
    using namespace interface;
    const auto version = interface_version();
    array_t methods{};
    append_methods<bitcoind_blockchain_methods>(methods, version);
    append_methods<bitcoind_control_methods>(methods, version);
    append_methods<bitcoind_mining_methods>(methods, version);
    append_methods<bitcoind_network_methods>(methods, version);
    append_methods<bitcoind_notifications_methods>(methods, version);
    append_methods<bitcoind_test_methods>(methods, version);
    append_methods<bitcoind_transaction_methods>(methods, version);
    append_methods<bitcoind_utility_methods>(methods, version);
    append_methods<bitcoind_wallet_methods>(methods, version);

    send_result(object_t
    {
        { "openrpc", std::string{ "1.4.1" } },
        { "info", object_t
        {
            { "title", server::interface::bitcoind::to_subversion(version) },
            { "version", server::interface::bitcoind::to_string(version) }
        } },
        { "methods", std::move(methods) }
    });
}

void protocol_bitcoind_control::handle_get_openrpc_info(const code& ec,
    rpc_interface::get_openrpc_info, bool) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_openrpc();
}

void protocol_bitcoind_control::handle_rpc_discover(const code& ec,
    rpc_interface::rpc_discover, bool) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_openrpc();
}

// Dispatch is synchronous on channel strand, so there is no set of in-flight
// commands to report (bitcoind lists its own call here).
void protocol_bitcoind_control::handle_get_rpc_info(const code& ec,
    rpc_interface::get_rpc_info) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(object_t
    {
        { "active_commands", array_t{} },
        { "logpath", from_path(server_config().log.log_file1()) }
    });
}

void protocol_bitcoind_control::handle_logging(const code& ec,
    rpc_interface::logging, const array_t& include,
    const array_t& exclude) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!include.empty() || !exclude.empty())
    {
        send_error(error::bitcoind::invalid_parameter);
        return;
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
    });
}

void protocol_bitcoind_control::handle_uptime(const code& ec,
    rpc_interface::uptime) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(possible_wide_cast<uint64_t>(floored_subtract(
        to_unsigned(zulu_time()), to_unsigned(start_time()))));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
