/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_electrum::handle_blockchain_estimate_fee(const code& ec,
    rpc_interface::blockchain_estimate_fee, double,
    const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    ////const auto mode_enabled = at_least(electrum::version::v1_6);

    ////send_result(number, 70, BIND(complete, _1));
    send_code(error::not_implemented);
}

void protocol_electrum::handle_blockchain_relay_fee(const code& ec,
    rpc_interface::blockchain_relay_fee) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0) ||
         at_least(electrum::version::v1_6))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(node_settings().minimum_fee_rate, 42, BIND(complete, _1));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
