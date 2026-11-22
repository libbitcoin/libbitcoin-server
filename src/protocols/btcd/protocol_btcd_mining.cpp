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
#include <bitcoin/server/protocols/protocol_btcd_mining.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/serializers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// btcd templates carry coinbase flags and latest block time, no bip9 rules.
void protocol_btcd_mining::finish_template(object_t& out) const NOEXCEPT
{
    constexpr auto max_time_offset = 2_u64 * 60_u64 * 60_u64;
    out.erase("rules");
    out.erase("vbavailable");
    out.erase("vbrequired");
    out["coinbaseaux"] = object_t{ { "flags", string_t{} } };
    out["maxtime"] = network::unix_time() + max_time_offset;
}

// btcd reports the top block's size, weight and tx count, and no mining.
void protocol_btcd_mining::do_mining_info(const code& ec,
    const database::tx_links& links, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec && ec != node::error::pooling_disabled)
    {
        send_error(error::btcd::internal_error);
        return;
    }

    size_t height{}, light{}, heavy{};
    const auto& query = archive();
    const auto link = query.to_top_confirmed();
    const auto top = query.get_header(link);
    if (!top || !query.get_height(height, link) ||
        !query.get_block_sizes(light, heavy, link))
    {
        send_error(error::btcd::internal_error);
        return;
    }

    const auto& bitcoin = system_settings();
    const auto span = to_floating(power2<uint64_t>(32u));
    const auto period = bitcoin.block_spacing_seconds;
    const auto limit = bitcoin.proof_of_work_limit;
    send_result(object_t
    {
        { "blocks", to64(height) },
        { "currentblocksize", to64(heavy) },
        { "currentblockweight", to64(chain::weighted_size(light, heavy)) },
        { "currentblocktx", to64(query.get_tx_count(link)) },
        { "difficulty", btcd::difficulty(top->bits(), limit) },
        { "errors", string_t{} },
        { "generate", false },
        { "genproclimit", 0 },
        { "hashespersec", 0.0 },
        { "networkhashps", top->difficulty() * span / period },
        { "pooledtx", to64(links.size()) },
        { "testnet", btcd::is_testnet(bitcoin) }
    });
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
