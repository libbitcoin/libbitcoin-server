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
#include <bitcoin/server/messages/subscription.hpp>

#include <chrono>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/messages/route.hpp>

namespace libbitcoin {
namespace server {

subscription::subscription(const subscription& other)
  : route(other),
    id_(other.id_),
    updated_(other.updated_.load()),
    sequence_(other.sequence_.load())
{
}

subscription::subscription(const route& return_route, uint32_t id, time_t now)
  : route(return_route),
    id_(id),
    updated_(now),
    sequence_(0)
{
}

uint32_t subscription::id() const
{
    return id_;
}

time_t subscription::updated() const
{
    return updated_;
}

void subscription::set_updated(time_t now) const
{
    updated_ = now;
}

void subscription::increment() const
{
    ++sequence_;
}

uint16_t subscription::sequence() const
{
    return sequence_;
}

// Thsi uses copy/swap idiom, see swap below.
subscription& subscription::operator=(subscription other)
{
    swap(*this, other);
    return *this;
}

bool subscription::operator<(const subscription& other) const
{
    return updated_ < other.updated_;
}

bool subscription::operator==(const subscription& other) const
{
    return delimited_ == other.delimited_ &&
        address_ == other.address_;
}

bool subscription::operator==(const route& other) const
{
    return delimited_ == other.delimited() &&
        address_ == other.address();
}

// friend function, see: stackoverflow.com/a/5695855/1172329
void swap(subscription& left, subscription& right)
{
    using std::swap;

    // Must be unqualified (no std namespace).
    swap(static_cast<route&>(left), static_cast<route&>(right));
    swap(left.id_, right.id_);

    // Swapping the atomics in assignment operator does not require atomicity.
    left.updated_ = right.updated_.exchange(left.updated_);
    left.sequence_ = right.sequence_.exchange(left.sequence_);
}

} // namespace server
} // namespace libbitcoin
