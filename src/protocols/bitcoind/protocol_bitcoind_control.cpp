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

// Isolate the subgroup dispatch metaprogramming to this translation unit.
template class network::rpc::dispatcher<
    server::interface::bitcoind_control>;

namespace server {

template class protocol_bitcoind_dispatch<interface::bitcoind_control>;

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
    SUBSCRIBE_BITCOIND(handle_get_memory_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_openrpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_rpc_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_logging, _1, _2);
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

bool protocol_bitcoind_control::handle_get_memory_info(const code& ec,
    rpc_interface::get_memory_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_control::handle_get_openrpc_info(const code& ec,
    rpc_interface::get_openrpc_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_control::handle_get_rpc_info(const code& ec,
    rpc_interface::get_rpc_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_control::handle_logging(const code& ec,
    rpc_interface::logging) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_control::handle_uptime(const code& ec,
    rpc_interface::uptime) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
