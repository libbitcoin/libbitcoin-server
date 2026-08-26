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
#include <bitcoin/server/protocols/protocol_btcd.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/serializers/serializers.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd

using namespace system;
using namespace network;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Handlers (getters).
// ----------------------------------------------------------------------------

// Required by btcwallet during wallet chain-sync bootstrap.
bool protocol_btcd::handle_get_best_block(const code& ec,
    btcd_interface::get_best_block) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    send_result(object_t
    {
        { "hash", encode_hash(query.get_top_confirmed_hash()) },
        { "height", query.get_top_confirmed() }
    }, 96);
    return true;
}

// Overrides the bitcoind method (btcd is attached first, so it claims the
// request). Softfork activation is configured, not assumable. Taproot is
// reported when configured active, with its configured activation height --
// lnd's backendSupportsTaproot requires the key's presence (not its field
// values) before treating any btcd backend as usable.
bool protocol_btcd::handle_get_block_chain_info(const code& ec,
    btcd_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t out{};
    if (!chain_info(out, archive(), system_settings(),
        node_settings().limited_blocks, is_current_chain(true)))
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    const auto& settings = system_settings();
    object_t soft_forks{};
    if (settings.forks.bip341 && settings.forks.bip342)
    {
        soft_forks.emplace("taproot", object_t
        {
            { "status", std::string{ "active" } },
            { "bit", 2 },
            { "startTime", -1 },
            { "timeout", -1 },
            { "since", settings.bip9_bit2_active_checkpoint.height() },
            { "min_activation_height", 0 }
        });
    }

    out.emplace("bip9_softforks", std::move(soft_forks));
    send_result(std::move(out), 512);
    return true;
}

// p2p magic (checked once by btcwallet/lnd to confirm network).
bool protocol_btcd::handle_get_current_net(const code& ec,
    btcd_interface::get_current_net) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(network_settings().identifier, 20);
    return true;
}

bool protocol_btcd::handle_get_difficulty(const code& ec,
    btcd_interface::get_difficulty) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    send_result(header->difficulty(), 20);
    return true;
}

bool protocol_btcd::handle_get_info(const code& ec,
    btcd_interface::get_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    // btcd's numeric version encoding (1'000'000 major, 10'000 minor, 100 patch).
    const auto& segments = server_settings().btcd.version.segments();
    const auto version = 1'000'000 * segments[0] + 10'000 * segments[1] +
        100 * segments[2];

    send_result(object_t
    {
        { "version", version },
        { "protocolversion", network_settings().protocol_maximum },
        { "blocks", top },
        { "timeoffset", 0 },
        { "connections", channel_count() },
        { "proxy", std::string{} },
        { "difficulty", header->difficulty() },
        { "testnet", chain_name(query) != "main" },
        { "relayfee", node_settings().minimum_fee_rate },
        { "errors", std::string{} }
    }, 256);
    return true;
}

bool protocol_btcd::handle_get_net_totals(const code& ec,
    btcd_interface::get_net_totals) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(object_t
    {
        { "totalbytesrecv", 0 },
        { "totalbytessent", 0 },
        { "timemillis", possible_wide_cast<int64_t>(zulu_time()) * 1'000 }
    }, 64);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
