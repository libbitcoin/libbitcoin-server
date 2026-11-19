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
#include <bitcoin/server/protocols/protocol_esplora.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_esplora

using namespace system;
using namespace system::json;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The confirmation targets of a fee-estimates response.
constexpr std::array<size_t, 28> targets
{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 144, 504, 1008
};

// The vsize of a fee histogram bin, and the count of recent txs (electrs).
constexpr size_t histogram_bin = 50'000;
constexpr size_t recent_count = 10;

bool protocol_esplora::handle_get_mempool(const code& ec, interface::mempool,
    uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    get_pool_fees(BIND(handle_mempool, _1, _2, gate()));
    return true;
}

bool protocol_esplora::handle_get_mempool_txids(const code& ec,
    interface::mempool_txids, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    get_pool_txs(BIND(handle_mempool_txids, _1, _2, gate()));
    return true;
}

bool protocol_esplora::handle_get_mempool_recent(const code& ec,
    interface::mempool_recent, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    get_pool_txs(BIND(handle_mempool_recent, _1, _2, gate()));
    return true;
}

bool protocol_esplora::handle_get_fee_estimates(const code& ec,
    interface::fee_estimates, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    estimates_.clear();
    next_estimate(zero, gate());
    return true;
}

// Completion handlers.
// ----------------------------------------------------------------------------

void protocol_esplora::handle_mempool(const code& ec,
    const database::fee_rates& rates, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_get_mempool, ec, rates, gate);
}

void protocol_esplora::complete_get_mempool(const code& ec,
    const database::fee_rates& rates, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || send_unlisted(ec))
        return;

    const auto summary = to_fee_summary(rates, histogram_bin);
    send_json(object
    {
        { "count", summary.count },
        { "vsize", summary.vsize },
        { "total_fee", summary.fee },
        { "fee_histogram", value_from(summary.histogram) }
    });
}

void protocol_esplora::handle_mempool_txids(const code& ec,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_get_mempool_txids, ec, links, gate);
}

void protocol_esplora::complete_get_mempool_txids(const code& ec,
    const database::tx_links& links, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || send_unlisted(ec))
        return;

    const auto& query = archive();

    array txids{};
    txids.reserve(links.size());
    for (const auto& link: links)
        txids.emplace_back(encode_hash(query.get_tx_key(link)));

    send_json(value{ std::move(txids) });
}

// The pool is ordered by entry, so the most recent are at its end.
void protocol_esplora::handle_mempool_recent(const code& ec,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_get_mempool_recent, ec, links, gate);
}

void protocol_esplora::complete_get_mempool_recent(const code& ec,
    const database::tx_links& links, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || send_unlisted(ec))
        return;

    const auto& query = archive();

    array recent{};
    const auto count = std::min(recent_count, links.size());
    const auto end = std::next(links.crbegin(), count);
    for (auto link = links.crbegin(); link != end; ++link)
    {
        size_t vsize{};
        uint64_t fee{}, spend{};
        if (!query.get_tx_fee(fee, *link) ||
            !query.get_tx_virtual_size(vsize, *link) ||
            !query.get_tx_spend(spend, *link))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        recent.push_back(object
        {
            { "txid", encode_hash(query.get_tx_key(*link)) },
            { "fee", fee },
            { "vsize", vsize },
            { "value", spend }
        });
    }

    send_json(value{ std::move(recent) });
}

// A closed pool has no listing, and is not found.
bool protocol_esplora::send_unlisted(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!ec)
        return false;

    if (ec == node::error::pooling_disabled)
        send_not_found();
    else
        send_internal_server_error(ec);

    return true;
}

void protocol_esplora::next_estimate(size_t index,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (index == targets.size())
    {
        send_json(std::move(estimates_));
        estimates_.clear();
        return;
    }

    // The estimator targets the next block as zero.
    estimate(sub1(targets.at(index)), node::estimate_mode::basic,
        BIND(handle_estimate, _1, _2, index, gate));
}

void protocol_esplora::handle_estimate(const code& ec, uint64_t fee,
    size_t index, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_estimate, ec, fee, index, gate);
}

constexpr double sats_per_vkbyte_to_sats_per_vbyte = 1'000.0;

void protocol_esplora::complete_estimate(const code& ec, uint64_t fee,
    size_t index, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    const auto disabled =
        ec == node::error::estimate_false ||
        ec == node::error::estimate_disabled ||
        ec == node::error::estimate_premature;

    if (!disabled && ec)
    {
        // node::error::estimates_failed, implies store fault.
        send_internal_server_error(ec);
        return;
    }

    // An unavailable target is omitted, as there is no null convention.
    if (!disabled)
        estimates_[system::serialize(targets.at(index))] =
            fee / sats_per_vkbyte_to_sats_per_vbyte;

    next_estimate(add1(index), gate);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
