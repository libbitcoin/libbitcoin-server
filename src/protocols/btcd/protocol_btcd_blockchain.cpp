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
#include <bitcoin/server/protocols/protocol_btcd_blockchain.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/serializers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// btcd reports neither median time, chain work, tx count nor target, and scales
// difficulty to network proof of work limit.
void protocol_btcd_blockchain::inject_block(json::object& out,
    const database::header_link& link,
    const chain::header& header) const NOEXCEPT
{
    base::inject_block(out, link, header);
    out.stable_erase("mediantime");
    out.stable_erase("chainwork");
    out.stable_erase("nTx");
    out.stable_erase("target");

    const auto limit = system_settings().proof_of_work_limit;
    out["difficulty"] = btcd::difficulty(header.bits(), limit);
}

// btcd embeds each tx with its block context, without fee or prevouts.
void protocol_btcd_blockchain::inject_block_tx(json::object& out,
    const chain::transaction& tx, bool, bool) const NOEXCEPT
{
    inject_scripts(out, tx);
    inject_context(out, archive().to_tx(tx.hash(false)));
}

void protocol_btcd_blockchain::finish_block(json::object& out) const NOEXCEPT
{
    auto txs = std::move(out.at("tx"));
    out.stable_erase("tx");
    out["rawtx"] = std::move(txs);
}

// btcd answers an output spent only by a pooled tx.
bool protocol_btcd_blockchain::is_spent(const database::output_link& link,
    bool) const NOEXCEPT
{
    return archive().is_confirmed_spent(link);
}

bool protocol_btcd_blockchain::verbose_mempool(json::value& out,
    const database::tx_links& links) const NOEXCEPT
{
    const auto& query = archive();
    json::object entries{};
    for (const auto& link: links)
    {
        json::object entry{};
        if (!btcd::mempool_entry(entry, query, link))
            return false;

        entries[encode_hash(query.get_tx_key(link))] = std::move(entry);
    }

    out = std::move(entries);
    return true;
}

// btcd's bytes are serialized sizes, these are virtual sizes.
bool protocol_btcd_blockchain::mempool_information(json::object& out,
    const database::fee_rates& rates, uint64_t, uint64_t,
    uint64_t) const NOEXCEPT
{
    size_t bytes{};
    for (const auto& rate: rates)
        bytes = ceilinged_add(bytes, rate.bytes);

    out = json::object
    {
        { "size", rates.size() },
        { "bytes", bytes }
    };

    return true;
}

void protocol_btcd_blockchain::handle_get_block_hash(const code& ec,
    rpc_interface::get_block_hash, double height) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    size_t block_height{};
    database::header_link link{};
    if (to_integer(block_height, height))
        link = query.to_confirmed(block_height);

    if (link.is_terminal())
    {
        send_error_message(error::btcd::misc_error,
            "Block number out of range");
        return;
    }

    send_result(encode_hash(query.get_header_key(link)));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
