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
#include <bitcoin/server/serializers/bitcoind_deployments.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A frozen activation (bitcoind's "buried" type, which assumes depth).
// bitcoind reports it as active from one block below the activation height
// (the rules are enforced for the block that follows).
static void push_frozen(object_t& out, const std::string& name, bool enabled,
    size_t activation, size_t height) NOEXCEPT
{
    if (!enabled)
        return;

    out.emplace(name, object_t
    {
        { "type", std::string{ "buried" } },
        { "active", add1(height) >= activation },
        { "height", activation }
    });
}

// Deployments are configured, so this reads settings and needs no chain state.
// Taproot is excluded, as bitcoind froze it and no longer reports it here
// (btcd reports it under getblockchaininfo's bip9_softforks, which lnd reads).
object_t deployment_info(const node::query& query,
    const system::settings& settings, const database::header_link& link,
    size_t height) NOEXCEPT
{
    const auto& forks = settings.forks;
    object_t deployments{};
    push_frozen(deployments, "bip34", forks.bip34,
        settings.bip90_bip34_height, height);
    push_frozen(deployments, "bip66", forks.bip66,
        settings.bip90_bip66_height, height);
    push_frozen(deployments, "bip65", forks.bip65,
        settings.bip90_bip65_height, height);
    push_frozen(deployments, "csv", forks.bip68 && forks.bip112 &&
        forks.bip113, settings.bip9_bit0_active_checkpoint.height(), height);
    push_frozen(deployments, "segwit", forks.bip141 && forks.bip143 &&
        forks.bip147, settings.bip9_bit1_active_checkpoint.height(), height);

    return object_t
    {
        { "hash", encode_hash(query.get_header_key(link)) },
        { "height", height },
        { "deployments", std::move(deployments) }
    };
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
