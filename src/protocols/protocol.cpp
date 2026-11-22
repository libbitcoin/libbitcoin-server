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
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

size_t protocol::get_pruned_height() const NOEXCEPT
{
    if (!node_settings().limited_blocks)
        return {};

    const auto& system_ = system_settings();
    const auto milestone = get_active_height(system_.milestone.hash());
    const auto checkpoint = get_active_height(system_.top_checkpoint().hash());
    return std::max(milestone, checkpoint);
}

// private
size_t protocol::get_active_height(const hash_digest& hash) const NOEXCEPT
{
    const auto& query = archive();
    const auto link = query.to_header(hash);
    chain::context ctx{};

    // Uses candidate header vs. confirmed block so stripping shows at heights
    // below top checkpoint and milestone while still syncing. May result in a
    // weak branch milestone block implying a pruned height that doesn't confirm
    // (ok), which can't happen for checkpoints. Can be mitigated by setting
    // stripped on object when it's a milestone or checked object and pruning is
    // enabled. Pruning can be enabled after storage of non-stripped blocks,
    // which does not strip them. False if bip141 is not active by milestone.
    return query.get_context(ctx, link) &&
        ctx.is_enabled(chain::bip141_rule) &&
        query.is_candidate_header(link) ? ctx.height : zero;
}

} // namespace server
} // namespace libbitcoin
