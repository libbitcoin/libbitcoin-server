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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The vsize of a fee histogram bin, and the count of recent txs (electrumx).
constexpr size_t histogram_bin = 30'000;
constexpr size_t recent_count = 10;

void protocol_electrum::handle_mempool_get_fee_histogram(const code& ec,
    rpc_interface::mempool_get_fee_histogram) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    get_pool_fees(BIND(handle_fee_histogram, _1, _2, gate()));
}

void protocol_electrum::handle_mempool_recent(const code& ec,
    rpc_interface::mempool_recent) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    get_pool_txs(BIND(handle_recent, _1, _2, gate()));
}

void protocol_electrum::handle_mempool_get_info(const code& ec,
    rpc_interface::mempool_get_info) NOEXCEPT
{
    if (stopped(ec))
        return;

    // Not documented, but replaces blockchain.relayfee.
    if (!at_least(electrum::version::v1_6))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    const auto& settings = node_settings().pool;
    send_result(object_t
    {
        // Even with a txpool or txgraph this value is fixed.
        { "mempoolminfee", settings.minimum_fee_rate },
        { "minrelaytxfee", settings.minimum_fee_rate },
        { "incrementalrelayfee", settings.minimum_fee_rate }
    });
}

// Completion handlers.
// ----------------------------------------------------------------------------

void protocol_electrum::handle_fee_histogram(const code& ec,
    const database::fee_rates& rates, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_fee_histogram, ec, rates, gate);
}

void protocol_electrum::complete_fee_histogram(const code& ec,
    const database::fee_rates& rates, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec)
    {
        using namespace error::electrum;
        send_code(translate(ec, daemon_error));
        return;
    }

    const auto bins = to_compact_fee_histogram(rates, histogram_bin);
    array_t histogram{};
    histogram.reserve(bins.size());
    for (const auto& [rate, size]: bins)
        histogram.emplace_back(array_t{ rate, to64(size) });

    send_result(std::move(histogram));
}

// The pool is ordered by entry, so the most recent are at its end.
void protocol_electrum::handle_recent(const code& ec,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_recent, ec, links, gate);
}

void protocol_electrum::complete_recent(const code& ec,
    const database::tx_links& links, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace error::electrum;

    if (stopped())
        return;

    if (ec)
    {
        send_code(translate(ec, daemon_error));
        return;
    }

    const auto& query = archive();

    array_t recent{};
    const auto count = std::min(recent_count, links.size());
    const auto end = std::next(links.crbegin(), count);
    for (auto link = links.crbegin(); link != end; ++link)
    {
        size_t vsize{};
        uint64_t fee{};
        if (!query.get_tx_fee(fee, *link) ||
            !query.get_tx_virtual_size(vsize, *link))
        {
            send_code(daemon_error);
            return;
        }

        recent.emplace_back(object_t
        {
            { "txid", encode_hash(query.get_tx_key(*link)) },
            { "fee", fee },
            { "vsize", to64(vsize) }
        });
    }

    send_result(std::move(recent));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
