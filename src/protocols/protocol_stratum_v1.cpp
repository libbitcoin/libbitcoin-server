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
#include <bitcoin/server/protocols/protocol_stratum_v1.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_stratum_v1

using namespace interface;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_stratum_v1::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Client requests.
    SUBSCRIBE_RPC(handle_mining_subscribe, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_mining_authorize, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_mining_submit, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_RPC(handle_mining_extranonce_subscribe, _1, _2);
    SUBSCRIBE_RPC(handle_mining_extranonce_unsubscribe, _1, _2, _3);

    // Server notifications.
    SUBSCRIBE_RPC(handle_mining_configure, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_set_difficulty, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_notify, _1, _2, _3, _4, _5, _6, _7, _8, _9,
        _10, _11, _12, _13);
    SUBSCRIBE_RPC(handle_client_reconnect, _1, _2, _3, _4, _5);
    SUBSCRIBE_RPC(handle_client_hello, _1, _2, _3);
    SUBSCRIBE_RPC(handle_client_rejected, _1, _2, _3, _4);
    protocol_rpc<channel_stratum_v1>::start();
}

// Handlers (client requests).
// ----------------------------------------------------------------------------

bool protocol_stratum_v1::handle_mining_subscribe(const code& ec,
    rpc_interface::mining_subscribe, const std::string& ,
    double ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_mining_authorize(const code& ec,
    rpc_interface::mining_authorize, const std::string& ,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_mining_submit(const code& ec,
    rpc_interface::mining_submit, const std::string& ,
    const std::string& , const std::string& ,
    double , const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_mining_extranonce_subscribe(const code& ec,
    rpc_interface::mining_extranonce_subscribe) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_mining_extranonce_unsubscribe(const code& ec,
    rpc_interface::mining_extranonce_unsubscribe, double ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

// Handlers (server notifications).
// ----------------------------------------------------------------------------

bool protocol_stratum_v1::handle_mining_configure(const code& ec,
    rpc_interface::mining_configure,
    const interface::object_t& ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_mining_set_difficulty(const code& ec,
    rpc_interface::mining_set_difficulty, double ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_mining_notify(const code& ec,
    rpc_interface::mining_notify, const std::string& ,
    const std::string& , const std::string& ,
    const std::string& , const interface::array_t& ,
    double , double , double , bool ,
    bool , bool ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_client_reconnect(const code& ec,
    rpc_interface::client_reconnect, const std::string& , double ,
    double ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_client_hello(const code& ec,
    rpc_interface::client_hello,
    const interface::object_t& ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

bool protocol_stratum_v1::handle_client_rejected(const code& ec,
    rpc_interface::client_rejected, const std::string& ,
    const std::string& ) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_code(error::not_implemented);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
