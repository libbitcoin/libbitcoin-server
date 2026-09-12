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

namespace libbitcoin {
namespace server {

#define CLASS protocol_esplora

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The confirmation targets of a fee-estimates response.
constexpr std::array<size_t, 28> targets
{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 144, 504, 1008
};

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

    // There is no tx pool.
    send_json(boost::json::object
    {
        { "count", 0 },
        { "vsize", 0 },
        { "total_fee", 0 },
        { "fee_histogram", boost::json::array{} }
    }, 128);
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

    // There is no tx pool.
    send_json(boost::json::array{}, 42);
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

    // There is no tx pool.
    send_json(boost::json::array{}, 42);
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
    next_estimate(zero);
    return true;
}

// Completion handlers.
// ----------------------------------------------------------------------------

void protocol_esplora::next_estimate(size_t index) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (index == targets.size())
    {
        send_json(std::move(estimates_), 512);
        estimates_.clear();
        return;
    }

    estimate(targets.at(index), node::estimator::mode::basic,
        BIND(handle_estimate, _1, _2, index));
}

void protocol_esplora::handle_estimate(const code& ec, uint64_t fee,
    size_t index) NOEXCEPT
{
    POST(complete_estimate, ec, fee, index);
}

void protocol_esplora::complete_estimate(const code& ec, uint64_t fee,
    size_t index) NOEXCEPT
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
        estimates_[serialize(targets.at(index))] = fee;

    next_estimate(add1(index));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
