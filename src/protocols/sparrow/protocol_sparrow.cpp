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
#include <bitcoin/server/protocols/protocol_sparrow.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_sparrow

using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Start.
// ----------------------------------------------------------------------------

void protocol_sparrow::start() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (started())
        return;

    SUBSCRIBE_RPC(handle_blockchain_block_stats, _1, _2, _3);
    SUBSCRIBE_RPC(handle_blockchain_silent_payments_subscribe, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_RPC(handle_blockchain_silent_payments_unsubscribe, _1, _2, _3, _4);
    protocol_rpc<interface::sparrow>::start();
}

// Handlers.
// ----------------------------------------------------------------------------

void protocol_sparrow::handle_blockchain_block_stats(const code& ec,
    rpc_interface::blockchain_block_stats, double) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: height -> { height, blockhash, feerate_percentiles, total_weight,
    // TODO: txs, time }, as the bitcoind getblockstats subset.
    send_code(error::electrum::method_not_found);
}

void protocol_sparrow::handle_blockchain_silent_payments_subscribe(
    const code& ec, rpc_interface::blockchain_silent_payments_subscribe,
    const std::string&, const std::string&, const interface::value_t&,
    const interface::array_t&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: validate the key pair (32 byte scan secret, 33 byte spend point),
    // TODO: bound by options().maximum_silent_payments, scan from start, and
    // TODO: notify progress/history on blockchain.silentpayments.subscribe.
    send_code(error::electrum::method_not_found);
}

void protocol_sparrow::handle_blockchain_silent_payments_unsubscribe(
    const code& ec, rpc_interface::blockchain_silent_payments_unsubscribe,
    const std::string&, const std::string&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: drop the subscription and return the scan address.
    send_code(error::electrum::method_not_found);
}

BC_POP_WARNING()

#undef CLASS

} // namespace server
} // namespace libbitcoin
