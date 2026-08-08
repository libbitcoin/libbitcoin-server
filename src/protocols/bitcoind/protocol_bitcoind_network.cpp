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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Network methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_rpc::handle_get_network_info(const code& ec,
    rpc_interface::get_network_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // bitcoind's numeric version encoding (10'000 major, 100 minor, patch).
    const auto& settings = server_settings().bitcoind;
    const auto& segments = settings.version.segments();
    const auto version = 10'000 * segments[0] + 100 * segments[1] + segments[2];

    send_result(object_t
    {
        { "version", version },
        { "subversion", settings.subversion },
        { "protocolversion", network_settings().protocol_maximum },
        { "localrelay", true },
        { "timeoffset", 0 },
        { "connections", 0 },
        { "networkactive", true },
        { "networks", array_t{} },
        { "relayfee", node_settings().minimum_fee_rate },
        { "incrementalfee", node_settings().minimum_bump_rate },
        { "localaddresses", array_t{} },
        { "warnings", std::string{} }
    }, 256);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
