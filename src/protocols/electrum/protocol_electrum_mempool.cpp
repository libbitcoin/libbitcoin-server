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

void protocol_electrum::handle_mempool_get_fee_histogram(const code& ec,
    rpc_interface::mempool_get_fee_histogram) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::wrong_version);
        return;
    }

    // TODO: could be simulated with block fees.
    send_result(array_t{}, 42);
    ////send_code(error::not_implemented);
}

void protocol_electrum::handle_mempool_get_info(const code& ec,
    rpc_interface::mempool_get_info) NOEXCEPT
{
    if (stopped(ec))
        return;

    // Not documented, but replaces blockchain.relayfee.
    if (!at_least(electrum::version::v1_6))
    {
        send_code(error::wrong_version);
        return;
    }

    const auto& settings = node_settings();
    send_result(object_t
    {
        // Even with a txpool or txgraph this value is fixed.
        { "mempoolminfee", settings.minimum_fee_rate },
        { "minrelaytxfee", settings.minimum_fee_rate },
        { "incrementalrelayfee", settings.minimum_bump_rate }
    }, 128);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
