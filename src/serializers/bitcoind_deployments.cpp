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

// An activation reported as bitcoind's "buried" type, which assumes depth.
// bitcoind reports it as active from one block below the activation height
// (the rules are enforced for the block that follows).
static void push_buried(object_t& out, const node::query& query,
    const std::string& name, bool enabled, chain::flags flag,
    size_t height) NOEXCEPT
{
    if (!enabled)
        return;

    size_t activation{};
    if (!query.get_activation(activation, flag))
    {
        out.emplace(name, object_t
        {
            { "type", std::string{ "buried" } },
            { "active", false }
        });

        return;
    }

    out.emplace(name, object_t
    {
        { "type", std::string{ "buried" } },
        { "active", add1(height) >= activation },
        { "height", to64(activation) }
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
    push_buried(deployments, query, "bip34", forks.bip34,
        chain::flags::bip34_rule, height);
    push_buried(deployments, query, "bip66", forks.bip66,
        chain::flags::bip66_rule, height);
    push_buried(deployments, query, "bip65", forks.bip65,
        chain::flags::bip65_rule, height);
    push_buried(deployments, query, "csv", forks.bip68 && forks.bip112 &&
        forks.bip113, chain::flags::bip68_rule, height);
    push_buried(deployments, query, "segwit", forks.bip141 && forks.bip143 &&
        forks.bip147, chain::flags::bip141_rule, height);

    return object_t
    {
        { "hash", encode_hash(query.get_header_key(link)) },
        { "height", to64(height) },
        { "deployments", std::move(deployments) }
    };
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
