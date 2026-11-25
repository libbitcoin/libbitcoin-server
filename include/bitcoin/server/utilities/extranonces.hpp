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
#ifndef LIBBITCOIN_SERVER_UTILITIES_EXTRANONCES_HPP
#define LIBBITCOIN_SERVER_UTILITIES_EXTRANONCES_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Random coinbase extranonce prefixes, none in use the start of another, so
/// no two mining channels search the same work. Thread safe.
class BCS_API extranonces
  : public std::enable_shared_from_this<extranonces>
{
public:
    typedef std::shared_ptr<extranonces> ptr;

    /// A prefix in use, released when the last reference is dropped.
    using prefix = std::shared_ptr<const system::data_chunk>;

    /// Attempts to draw a prefix not overlapping one in use.
    static constexpr size_t maximum_attempts = 256;

    /// A prefix of the size, empty if zero size or none was drawn.
    prefix allocate(size_t size) NOEXCEPT;

    /// The number of prefixes in use.
    size_t size() const NOEXCEPT;

private:
    bool overlaps(const system::data_chunk& candidate) const NOEXCEPT;
    void release(const system::data_chunk& value) NOEXCEPT;

    // These are protected by mutex.
    std::set<system::data_chunk> used_{};
    mutable std::mutex mutex_{};
};

} // namespace server
} // namespace libbitcoin

#endif
