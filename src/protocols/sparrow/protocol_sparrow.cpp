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

// A method the electrum interface does not define is the sparrow interface,
// or is not served at all (the electrum terminal responds).
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

// Silent payment (bip352) support, as the version list frigate publishes.
void protocol_sparrow::add_features(object_t& features) const NOEXCEPT
{
    features["silent_payments"] = array_t{ silent_payments_version };
}

// Handlers (stubs, claimed but not yet bound to the store).
// ----------------------------------------------------------------------------
// github.com/sparrowwallet/frigate ElectrumServerService

void protocol_sparrow::handle_blockchain_block_stats(const code& ec,
    sparrow_interface::blockchain_block_stats, double) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: height -> { height, blockhash, feerate_percentiles, total_weight,
    // TODO: txs, time }.
    send_code(error::electrum::method_not_found);
}

// The client sends the scan secret, never the spend secret (bip352).
void protocol_sparrow::handle_blockchain_silent_payments_subscribe(
    const code& ec, sparrow_interface::blockchain_silent_payments_subscribe,
    const std::string&, const std::string&, const interface::value_t&,
    const interface::array_t&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: validate the key pair (32 byte secret, 33 byte point), bound by
    // TODO: maximum_subscriptions, scan from start, notify progress/history.
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
