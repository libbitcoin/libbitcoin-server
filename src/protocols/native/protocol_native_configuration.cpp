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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <atomic>
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

    using namespace system;
    using selection = config::selection;
    const auto chain = selection::to_word(system_settings().genesis_network);
    const auto& query = archive();
    const auto& node = node_settings();
    const auto& peer = network_settings();
    const auto& fork = system_settings().forks;
    const auto& wallet = server_settings().wallet;
    const auto silent = query.silent_enabled() ?
        json::value(query.silent_start_height()) : json::value(nullptr);

    send_json(json::object
    {
        { "store", json::object
            {
                { "address", query.address_enabled() },
                { "filter", query.filter_enabled() },
                { "silent", silent },
                { "turbo", database_settings().turbo },
                { "witness", node.require_witness },
                { "pruned", get_pruned_height() }
            }
        },
        { "node", json::object
            {
                { "network", std::string{ chain } },
                { "limited", node.limited_blocks },
                { "blocks", node.provide_blocks },
                { "witness", node.provide_witness },
                { "filters", node.provide_filters },
                { "privacy", node.provide_privacy },
                { "fees", peer.enable_relay }
            }
        },
        { "wallet", json::object
            {
                { "p2kh", uint8_t{ wallet.p2kh_prefix } },
                { "p2sh", uint8_t{ wallet.p2sh_prefix } },
                { "wif", uint8_t{ wallet.wif_prefix } },
                { "witness", wallet.witness_prefix },
                { "silent", wallet.silent_prefix },
                { "hd_private", wallet.hd_private_prefix },
                { "hd_public", wallet.hd_public_prefix }
            }
        },
        { "network", json::object
            {
                { "relay", peer.enable_relay },
                { "alert", peer.enable_alert },
                { "reject", peer.enable_reject },
                { "compact", peer.enable_compact },
                { "address", peer.enable_address },
                { "witness_tx", peer.enable_witness_tx },
                { "memory_pool", peer.enable_memory_pool },
                { "not_found", peer.enable_not_found },
                { "protocol_maximum", peer.protocol_maximum },
                { "protocol_minimum", peer.protocol_minimum }
            }
        },
        { "forks", json::object
            {
                { "bip16", fork.bip16 },
                { "bip90", fork.bip90 },
                { "bip30", fork.bip30 },
                { "bip42", fork.bip42 },
                { "bip34", fork.bip34 },
                { "bip65", fork.bip65 },
                { "bip66", fork.bip66 },
                { "bip68", fork.bip68 },
                { "bip112", fork.bip112 },
                { "bip113", fork.bip113 },
                { "bip141", fork.bip141 },
                { "bip143", fork.bip143 },
                { "bip147", fork.bip147 },
                { "bip341", fork.bip341 },
                { "bip342", fork.bip342 },
                { "bip54_timewarp", fork.bip54_timewarp },
                { "bip54_timespan", fork.bip54_timespan },
                { "bip54_sigops", fork.bip54_sigops },
                { "bip54_txsize", fork.bip54_txsize },
                { "bip54_coinbase", fork.bip54_coinbase },
                { "retarget", fork.retarget },
                { "difficult", fork.difficult },
                { "time_warp_patch", fork.time_warp_patch },
                { "block_storm_patch", fork.block_storm_patch },
                { "signet", fork.signet },
                { "ltc_time_warp_patch", fork.ltc_time_warp_patch },
                { "ltc_retarget_overflow_patch", fork.ltc_retarget_overflow_patch },
                { "ltc_scrypt_proof_of_work", fork.ltc_scrypt_proof_of_work }
            }
        }
    });
    return true;
}

// utility
// ----------------------------------------------------------------------------

size_t protocol_native::get_pruned_height() NOEXCEPT
{
    if (!node_settings().limited_blocks)
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
