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

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd

// protocol_bitcoind_rpc declares 'using post = network::http::method::post',
// which shadows network::protocol::post<Derived>. Qualify explicitly.
#define POST_BTCD(method, ...) \
    this->network::protocol::template post<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Filter parsing (loadtxfilter).
// ----------------------------------------------------------------------------

code protocol_btcd::parse_filter_keys(hashes& out,
    const value_t& addresses) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!std::holds_alternative<array_t>(addresses.value()))
        return error::invalid_argument;

    for (const auto& item: std::get<array_t>(addresses.value()))
    {
        if (!std::holds_alternative<string_t>(item.value()))
            return error::invalid_argument;

        chain::script script{};
        if (const auto ec = parse_output_script(
            std::get<string_t>(item.value()), script))
            return ec;

        out.push_back(script.hash());
    }

    return error::success;
}

code protocol_btcd::parse_filter_points(chain::points& out,
    const value_t& outpoints) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!std::holds_alternative<array_t>(outpoints.value()))
        return error::invalid_argument;

    for (const auto& item: std::get<array_t>(outpoints.value()))
    {
        if (!std::holds_alternative<object_t>(item.value()))
            return error::invalid_argument;

        const auto& fields = std::get<object_t>(item.value());
        const auto hash_it = fields.find("hash");
        const auto index_it = fields.find("index");
        if (hash_it == fields.end() || index_it == fields.end() ||
            !std::holds_alternative<string_t>(hash_it->second.value()) ||
            !std::holds_alternative<number_t>(index_it->second.value()))
            return error::invalid_argument;

        uint32_t index{};
        hash_digest hash{};
        if (!decode_hash(hash, std::get<string_t>(hash_it->second.value())) ||
            !to_integer(index, std::get<number_t>(index_it->second.value())))
            return error::invalid_argument;

        out.emplace_back(hash, index);
    }

    return error::success;
}

// Handlers (filters).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_load_tx_filter(const code& ec,
    btcd_interface::load_tx_filter, bool reload, const value_t& addresses,
    const value_t& outpoints) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hashes keys{};
    if (const auto fault = parse_filter_keys(keys, addresses))
    {
        send_error(fault);
        return true;
    }

    chain::points points{};
    if (const auto fault = parse_filter_points(points, outpoints))
    {
        send_error(fault);
        return true;
    }

    if (!keys.empty() && !archive().address_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_load_tx_filter, reload, std::move(keys),
        std::move(points));
    return true;
}

void protocol_btcd::do_load_tx_filter(bool reload, const hashes& keys,
    const chain::points& points) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    if (reload)
    {
        address_watches_.clear();
        outpoint_watches_.clear();
    }

    code ec{ error::success };
    const auto& query = archive();
    const auto maximum = server_settings().btcd.maximum_filters;
    const auto limit = server_settings().btcd.maximum_history;

    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        if (address_watches_.size() + outpoint_watches_.size() >= maximum)
        {
            ec = error::subscription_limit;
            break;
        }

        // Prime the cursor to present, so matching reports new blocks only.
        const auto at = address_watches_.try_emplace(key, address_watch{});
        if (at.second)
        {
            histories discard{};
            const auto fault = query.get_history(stopping_,
                at.first->second.cursor, discard, key, limit, turbo_);
            if (fault == database::error::query_canceled)
                return;
        }
    }

    for (const auto& prevout: points)
    {
        if (stopping_)
            return;

        if (ec)
            break;

        if (address_watches_.size() + outpoint_watches_.size() >= maximum)
        {
            ec = error::subscription_limit;
            break;
        }

        // Prime the spender set to present, so matching reports new only.
        const auto at = outpoint_watches_.try_emplace(prevout,
            outpoint_watch{});
        if (at.second)
        {
            auto& sub = at.first->second;
            sub.outpoint = query.get_tx_history(query.to_tx(prevout.hash()));
            sub.spenders = query.get_spenders_history(prevout);
        }
    }

    POST_BTCD(complete_load_tx_filter, ec);
}

void protocol_btcd::complete_load_tx_filter(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        send_error(ec);
        return;
    }

    send_result({}, 4);
}

bool protocol_btcd::handle_rescan_blocks(const code& ec,
    btcd_interface::rescan_blocks, const value_t& blockhashes) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!std::holds_alternative<array_t>(blockhashes.value()))
    {
        send_error(error::invalid_argument);
        return true;
    }

    hashes block_hashes{};
    for (const auto& item: std::get<array_t>(blockhashes.value()))
    {
        hash_digest hash{};
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            send_error(error::invalid_argument);
            return true;
        }

        block_hashes.push_back(hash);
    }

    if (!archive().address_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_rescan_blocks, std::move(block_hashes));
    return true;
}

// Snapshot the watch-list, so the query runs parallel (not on the strand).
void protocol_btcd::do_rescan_blocks(const hashes& block_hashes) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    hashes keys{};
    keys.reserve(address_watches_.size());
    for (const auto& watch: address_watches_)
        keys.push_back(watch.first);

    chain::points points{};
    points.reserve(outpoint_watches_.size());
    for (const auto& watch: outpoint_watches_)
        points.push_back(watch.first);

    PARALLEL(do_rescan_watches, block_hashes, std::move(keys),
        std::move(points));
}

void protocol_btcd::do_rescan_watches(const hashes& block_hashes,
    const hashes& keys, chain::points& points) NOEXCEPT
{
    BC_ASSERT(!stranded());

    // Resolve named blocks to heights (result retains request order).
    sizes heights{};
    std::vector<std::pair<hash_digest, size_t>> named{};
    const auto& query = archive();
    for (const auto& hash: block_hashes)
    {
        if (stopping_)
            return;

        size_t height{};
        if (!query.get_height(height, query.to_header(hash)))
        {
            POST_BTCD(complete_rescan_blocks, error::not_found, array_t{});
            return;
        }

        heights.insert(height);
        named.emplace_back(hash, height);
    }

    // Match the snapshot against the named heights (full replay).
    matches matched{};
    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        address_watch replay{};
        const auto fault = match_addresses(matched, replay, key, heights);
        if (fault == database::error::query_canceled)
            return;
    }

    for (const auto& prevout: points)
    {
        if (stopping_)
            return;

        outpoint_watch replay{};
        match_outpoints(matched, replay, prevout, heights);
    }

    array_t discovered{};
    for (const auto& [hash, height]: named)
    {
        if (stopping_)
            return;

        const auto at = matched.find(height);
        if (at != matched.end() && !at->second.empty())
        {
            discovered.emplace_back(object_t
            {
                { "hash", encode_hash(hash) },
                { "transactions", serialize_matches(at->second) }
            });
        }
    }

    POST_BTCD(complete_rescan_blocks, error::success, std::move(discovered));
}

void protocol_btcd::complete_rescan_blocks(const code& ec,
    const array_t& discovered) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        send_error(ec);
        return;
    }

    send_result(discovered, 256);
}

// Notification event handlers.
// ----------------------------------------------------------------------------

void protocol_btcd::do_connected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    const database::header_link link{ link_value };
    const auto& query = archive();

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    // Match the watch-list against the connected block (cursored delta).
    // Cursors advance here, so this stays on the notification strand.
    matches matched{};
    const sizes heights{ height };
    for (auto& [key, sub]: address_watches_)
    {
        if (stopping_)
            return;

        const auto fault = match_addresses(matched, sub, key, heights);
        if (fault == database::error::query_canceled)
            return;
    }

    for (auto& [prevout, sub]: outpoint_watches_)
    {
        if (stopping_)
            return;

        match_outpoints(matched, sub, prevout, heights);
    }

    array_t txs{};
    const auto at = matched.find(height);
    if (at != matched.end())
        txs = serialize_matches(at->second);

    POST_BTCD(notify_connected, encode_hash(query.get_header_key(link)),
        height, header->timestamp(),
        to_text(*header, chain::header::serialized_size()), std::move(txs));
}

void protocol_btcd::do_disconnected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    // Reset watch cursors, disconnected blocks invalidate the walks.
    for (auto& [key, sub]: address_watches_)
        sub.cursor = {};

    const database::header_link link{ link_value };
    const auto& query = archive();

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    POST_BTCD(notify_disconnected, encode_hash(query.get_header_key(link)),
        height, header->timestamp(),
        to_text(*header, chain::header::serialized_size()));
}

// blockconnected [hash, height, time] and filteredblockconnected [height,
// header, subscribedtxs], the latter sent unconditionally (as btcd).
void protocol_btcd::notify_connected(const std::string& hash,
    size_t height, uint32_t time, const std::string& header,
    const array_t& txs) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(std::memory_order_relaxed))
        return;

    send_notification("blockconnected", array_t
    {
        hash,
        height,
        time
    }, 256);

    send_notification("filteredblockconnected", array_t
    {
        height,
        header,
        txs
    }, 256);
}

// blockdisconnected [hash, height, time] and filteredblockdisconnected
// [height, header].
void protocol_btcd::notify_disconnected(const std::string& hash,
    size_t height, uint32_t time, const std::string& header) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(std::memory_order_relaxed))
        return;

    send_notification("blockdisconnected", array_t
    {
        hash,
        height,
        time
    }, 256);

    send_notification("filteredblockdisconnected", array_t
    {
        height,
        header
    }, 256);
}

// Utilities.
// ----------------------------------------------------------------------------

// Called from notification strand (live) and parallel (rescan), so unasserted.
code protocol_btcd::match_addresses(matches& out, address_watch& sub,
    const hash_digest& key, const sizes& heights) NOEXCEPT
{
    histories delta{};
    const auto& query = archive();
    const auto limit = server_settings().btcd.maximum_history;
    const auto ec = query.get_history(stopping_, sub.cursor, delta, key,
        limit, turbo_);
    if (ec)
        return ec;

    for (const auto& entry: delta)
        if (entry.confirmed() && heights.contains(entry.tx.height()))
            out[entry.tx.height()].emplace(entry.position, entry.tx.hash());

    return error::success;
}

void protocol_btcd::match_outpoints(matches& out, outpoint_watch& sub,
    const point& prevout, const sizes& heights) NOEXCEPT
{
    outpoint_watch next{};
    const auto& query = archive();
    next.outpoint = query.get_tx_history(query.to_tx(prevout.hash()));
    next.spenders = query.get_spenders_history(prevout);

    for (const auto& spender: difference(next.spenders, sub.spenders))
        if (spender.confirmed() && heights.contains(spender.tx.height()))
            out[spender.tx.height()].emplace(spender.position,
                spender.tx.hash());

    sub = std::move(next);
}

array_t protocol_btcd::serialize_matches(const matched_txs& txs) NOEXCEPT
{
    constexpr auto witness = true;
    array_t out{};
    const auto& query = archive();
    for (const auto& [position, hash]: txs)
    {
        if (stopping_)
            return out;

        const auto tx = query.get_transaction(query.to_tx(hash), witness);
        if (tx)
            out.emplace_back(to_text(*tx,
                tx->serialized_size(witness), witness));
    }

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
