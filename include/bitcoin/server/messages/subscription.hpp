/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_SUBSCRIPTION_HPP
#define LIBBITCOIN_SERVER_SUBSCRIPTION_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/route.hpp>

namespace libbitcoin {
namespace server {

/// This class is threadsafe and pretends to be const.
/// Caller must maange the race between increment and sequence.
class BCS_API subscription
  : public route
{
public:
    /// Copy constructor, required for bimap.
    subscription(const subscription& other);

    /// Construct subscription state from an existing route.
    subscription(const route& return_route, uint32_t id, time_t now);

    /// Arbitrary caller data, returned to caller on each notification.
    uint32_t id() const;

    /// Last subscription time, used for expirations.
    time_t updated() const;

    /// Renew subscription time to delay expiration.
    /// This is mutable so that change does not force a hash table update.
    void set_updated(time_t now) const;

    /// Increment sequence, indicating a send attempt.
    /// This is mutable so that change does not force a hash table update.
    void increment() const;

    /// The ordinal of the current subscription instance.
    uint16_t sequence() const;

    /// Assignment required for bimap equal_range.
    subscription& operator=(const subscription other);

    /// Age (only) comparison, required for bimap multiset ordering.
    bool operator<(const subscription& other) const;

    /// Equality comparison, required for bimap search by subscription.
    bool operator==(const subscription& other) const;

    /// Equality comparison, required for bimap search by route.
    bool operator==(const route& other) const;

    // Swap implementation required to properly handle base class.
    friend void swap(subscription& left, subscription& right);

protected:
    uint32_t id_;
    mutable std::atomic<time_t> updated_;
    mutable std::atomic<uint16_t> sequence_;
};

} // namespace server
} // namespace libbitcoin

#endif
