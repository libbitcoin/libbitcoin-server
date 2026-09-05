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
#include <bitcoin/server/protocols/protocol.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Broadcast tx retention.
// ----------------------------------------------------------------------------

void protocol::retain_tx(const chain::transaction::cptr& tx) NOEXCEPT
{
    BC_ASSERT(stranded());

    auto hash = tx->hash(false);
    if (retained_tx(hash))
        return;

    if (retained_.size() >= maximum_retained)
        retained_.pop_front();

    retained_.emplace_back(std::move(hash), tx);
}

const protocol::retained_txs& protocol::retained() const NOEXCEPT
{
    BC_ASSERT(stranded());
    return retained_;
}

chain::transaction::cptr protocol::retained_tx(
    const hash_digest& hash) const NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto match = [&hash](const auto& entry) NOEXCEPT
    {
        return entry.first == hash;
    };

    const auto it = std::find_if(retained_.begin(), retained_.end(), match);
    return it == retained_.end() ? nullptr : it->second;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
