/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <atomic>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;

#define CLASS protocol_native

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

bool protocol_native::handle_get_configuration(const code& ec,
    interface::configuration, uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    send_json(boost::json::object
    {
        { "address", archive().address_enabled() },
        { "filter", archive().filter_enabled() },
        { "turbo", database_settings().turbo },
        { "witness", network_settings().witness_node() },
        { "retarget", system_settings().forks.retarget },
        { "difficult", system_settings().forks.difficult },
        { "stripped", get_stripped_height() },
    }, 64);
    return true;
}

// utility
// ----------------------------------------------------------------------------

size_t protocol_native::get_stripped_height() NOEXCEPT
{
    if (!network_settings().pruned_node() ||
        !network_settings().witness_node())
        return {};

    const auto& system_ = system_settings();
    const auto milestone = get_active_height(system_.milestone.hash());
    const auto checkpoint = get_active_height(system_.top_checkpoint().hash());
    return std::max(milestone, checkpoint);
}

size_t protocol_native::get_active_height(
    const system::hash_digest& hash) NOEXCEPT
{
    const auto& query = archive();
    const auto link = query.to_header(hash);
    system::chain::context ctx{};

    // This uses candidate header vs. confirmed block so that stripping will
    // show at heights below top checkpoint and milestone while still syncing.
    // This can result in a weak branch milestone block implying a pruned
    // height that doesn't confirm (ok), which can't happen for checkpoints.
    // This can be mitigated by setting stripped on the object when it's a
    // milestone or checked object and pruning is enabled. Pruning can be
    // enabled after storage of non-stripped blocks, which does not strip them.
    // This will return false is bip141 is not active by the milestone.
    return query.get_context(ctx, link) && 
        ctx.is_enabled(system::chain::bip141_rule) && 
        query.is_candidate_header(link) ? ctx.height : zero;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
