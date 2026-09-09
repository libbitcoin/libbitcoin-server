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
#define SUBSCRIBE_SPARROW(method, ...) \
    sparrow_subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

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

    SUBSCRIBE_SPARROW(handle_blockchain_block_stats, _1, _2, _3);
    SUBSCRIBE_SPARROW(handle_blockchain_silent_payments_subscribe, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_SPARROW(handle_blockchain_silent_payments_unsubscribe, _1, _2, _3, _4);

    // Subscribes the inherited electrum interface and starts the protocol.
    protocol_electrum::start();
}

void protocol_sparrow::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    sparrow_dispatcher_.stop(ec);
    protocol_electrum::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

// A method the electrum interface does not define, which is the sparrow
// interface or (as the terminal responder) not served at all.
void protocol_sparrow::handle_unclaimed(const request_t& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!sparrow_dispatcher::contains(message.method))
    {
        protocol_electrum::handle_unclaimed(message);
        return;
    }

    if (const auto ec = sparrow_dispatcher_.notify(message))
        stop(ec);
}

// Features.
// ----------------------------------------------------------------------------

// Advertise the silent payment (bip352) protocol versions served, as the
// integer version list of the electrum server.features response. This is a
// property of the service, so it is not configured (electrum omits it).
void protocol_sparrow::add_features(object_t& features) const NOEXCEPT
{
    features["silent_payments"] = array_t{ silent_payments_version };
}

// Handlers.
// ----------------------------------------------------------------------------
// These are stubs. The methods are claimed by this protocol (so they are not
// answered as unknown by the electrum terminal responder), and answered as
// unimplemented until the block statistics and silent payment scan queries
// are bound to the store.

// github.com/sparrowwallet/frigate ElectrumServerService.getBlockStats
void protocol_sparrow::handle_blockchain_block_stats(const code& ec,
    sparrow_interface::blockchain_block_stats, double) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: height -> { height, blockhash, feerate_percentiles, total_weight,
    // TODO: txs, time }, as the bitcoind getblockstats subset.
    send_code(error::electrum::method_not_found);
}

// The scan secret is sent by the client and the spend secret is not, so this
// is a scan-only key pair (bip352). A secure transport is the operator's
// policy (configured binds), and is not enforced here.
void protocol_sparrow::handle_blockchain_silent_payments_subscribe(
    const code& ec, sparrow_interface::blockchain_silent_payments_subscribe,
    const std::string&, const std::string&, const interface::value_t&,
    const interface::array_t&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: validate the key pair (32 byte scan secret, 33 byte spend point),
    // TODO: bound by the electrum maximum_subscriptions, scan from start,
    // TODO: and notify progress/history on blockchain.silentpayments.subscribe.
    send_code(error::electrum::method_not_found);
}

void protocol_sparrow::handle_blockchain_silent_payments_unsubscribe(
    const code& ec, sparrow_interface::blockchain_silent_payments_unsubscribe,
    const std::string&, const std::string&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: drop the subscription and return the scan address.
    send_code(error::electrum::method_not_found);
}

BC_POP_WARNING()

#undef SUBSCRIBE_SPARROW
#undef CLASS

} // namespace server
} // namespace libbitcoin
