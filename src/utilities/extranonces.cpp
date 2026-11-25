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
#include <bitcoin/server/utilities/extranonces.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(NO_NEW_OR_DELETE)

// Value begins with start (or is equal to it).
static bool begins(const data_chunk& value, const data_chunk& start) NOEXCEPT
{
    return value.size() >= start.size() &&
        std::equal(start.begin(), start.end(), value.begin());
}

extranonces::prefix extranonces::allocate(size_t size) NOEXCEPT
{
    if (is_zero(size))
        return {};

    std::lock_guard lock(mutex_);
    for (size_t attempt{}; attempt < maximum_attempts; ++attempt)
    {
        data_chunk candidate(size);
        maybe_random::fill(candidate);
        if (overlaps(candidate))
            continue;

        used_.insert(candidate);
        const auto self = shared_from_this();
        return prefix(new data_chunk(std::move(candidate)),
            [self](const data_chunk* value) NOEXCEPT
            {
                self->release(*value);
                delete value;
            });
    }

    return {};
}

size_t extranonces::size() const NOEXCEPT
{
    std::lock_guard lock(mutex_);
    return used_.size();
}

// private
// Prefixes in use do not overlap, so only the neighbors of a candidate in
// order can be its start or begin with it.
bool extranonces::overlaps(const data_chunk& candidate) const NOEXCEPT
{
    const auto next = used_.lower_bound(candidate);
    if (next != used_.end() && begins(*next, candidate))
        return true;

    return next != used_.begin() && begins(candidate, *std::prev(next));
}

// private
void extranonces::release(const data_chunk& value) NOEXCEPT
{
    std::lock_guard lock(mutex_);
    used_.erase(value);
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
